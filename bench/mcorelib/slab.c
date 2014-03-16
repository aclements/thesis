#include "mcorelib.h"

#include <assert.h>
#include <stdlib.h>

// XXX Should bufctl's be side-banded?  The current inlining creates
// huge internal fragmentation for cache-packed structures.
// Side-banding them makes mapping between objects and bufctl's
// harder (http://gmplib.org/~tege/divcnst-pldi94.pdf).

// XXX During large allocation runs, most of the blocks we get from
// the block allocator will be zeroed by the system.  Take advantage
// of this.

enum {
        // The maximum number of objects to cache per CPU
        MAGAZINE_SIZE = 1024,
        // The number of objects to fetch when refilling a magazine
        MAGAZINE_REFILL = MAGAZINE_SIZE / 2,
        // The number of objects to leave in a magazine when flushing
        MAGAZINE_FLUSH = MAGAZINE_SIZE / 2,
};

// Lock order:
//   struct SlabPerCPU.lock
// > struct Slab_Cache.lock

// Byte offset of the next free bufCtl_t relative to the containing
// Slab, or 0 if this is the end of the list.  Using a byte offset
// avoids multiplication by the element size and lets us keep the type
// small.
typedef unsigned short bufCtl_t;

struct perCPU
{
        // Generally this lock should be uncontended, but it's
        // important if we get migrated in the middle of an operation.
        TicketLock_t lock;
        // A LIFO cache of recently freed objects.  These BufCtl's
        // point to entire free lists, not individual objects.
        bufCtl_t *mag[MAGAZINE_SIZE];
        // The next free slot in mag.
        unsigned int magNext;
        // A given entry in the magazine can point to a whole free
        // list.  This tracks how many free objects we have.
        unsigned int numFree;
};

struct slabList
{
        struct slabList *next, *prev;
};

struct slab
{
        struct slabList list;
        struct Slab_Cache *cache;
        // The first free slot, or 0 if none.
        bufCtl_t freeBuf;
        unsigned short numFree;
};

struct Slab_Cache
{
        struct slabList partial, full, empty;
        TicketLock_t lock;
        // The element size, including the bufCtl_t.  XXX objSize
        unsigned short eltSize;
        // The byte offset of the first bufCtl_t in a slab.
        unsigned short bufCtlStart;
        // The number of elements per slab.  XXX objsPerSlab
        unsigned short eltsPerSlab;
        const char *name;
        struct perCPU pc[0];
};

static inline void *getMagObj(struct perCPU *pc);
static inline void putMagObj(struct perCPU *pc, void *obj);
static void refillMag(struct Slab_Cache *cache, struct perCPU *pc);
static struct slab *getSlab(struct Slab_Cache *cache);
static struct slab *newSlab(struct Slab_Cache *cache);
static void flushMag(struct Slab_Cache *cache, struct perCPU *pc);
static void putSlabObj(struct Slab_Cache *cache, void *obj);

static inline void *
objOfBufCtl(bufCtl_t *bc)
{
        return (void*)bc + sizeof *bc;
}

static inline bufCtl_t *
bufCtlOfObj(void *obj)
{
        return obj - sizeof(bufCtl_t);
}

static inline struct slab *
slabOfObj(void *obj)
{
        return Block_Begin(obj);
}

static inline void
slabListInit(struct slabList *list)
{
        list->next = list;
        list->prev = list;
}

static inline void
slabUnlink(struct slab *slab)
{
        slab->list.next->prev = slab->list.prev;
        slab->list.prev->next = slab->list.next;
}

static inline void
slabListPush(struct slabList *list, struct slab *slab)
{
        slab->list.next = list->next;
        slab->list.prev = list;
        list->next->prev = &slab->list;
        list->next = &slab->list;
}

static inline struct slab *
slabListTryPop(struct slabList *list)
{
        struct slabList *next = list->next;
        if (next == list)
                return NULL;
        struct slab *slab = (void*)next - __builtin_offsetof(struct slab, list);
        slabUnlink(slab);
        return slab;
}

struct Slab_Cache *
Slab_CreateEx(size_t eltSize, const char *name)
{
        struct Slab_Cache *c;
        int cpus = CPU_MaxPossible();

        // XXX Alignment
        if (eltSize > BLOCK_SIZE - sizeof(struct slab))
                panic("Element size %u too large for slab allocator",
                      (unsigned)eltSize);

        c = calloc(1, sizeof *c + sizeof(c->pc[0]) * cpus);
        if (!c)
                return NULL;

        slabListInit(&c->partial);
        slabListInit(&c->full);
        slabListInit(&c->empty);
        c->eltSize = eltSize + sizeof(bufCtl_t);
        c->bufCtlStart = sizeof(struct slab);
        c->eltsPerSlab = (BLOCK_SIZE - c->bufCtlStart) / c->eltSize;
        c->name = name;
        return c;
}

void *
Slab_Alloc(struct Slab_Cache *cache)
{
        // Allocate from my local magazine
        struct perCPU *pc = &cache->pc[CPU_Current()];
        TicketLock_Lock(&pc->lock);
        if (!pc->magNext)
                refillMag(cache, pc);
        void *obj = getMagObj(pc);
        TicketLock_Unlock(&pc->lock);
        return obj;
}

// Pull an object off a magazine's free list.  The caller must hold
// the magazine lock and must ensure that the magazine is not empty.
static inline void *
getMagObj(struct perCPU *pc)
{
        bufCtl_t *bc = pc->mag[pc->magNext - 1];
        if (!*bc) {
                // We reached the end of that free list
                pc->magNext--;
        } else {
                // Unlink
                pc->mag[pc->magNext - 1] = Block_Begin(bc) + *bc;
        }
        pc->numFree--;
        return objOfBufCtl(bc);
}

// Put an object on a magazine's free list.  The caller must hold the
// magazine lock and must ensure that the magazine will not overflow.
static inline void
putMagObj(struct perCPU *pc, void *obj)
{
        pc->mag[pc->magNext++] = bufCtlOfObj(obj);
}

// Refill a per-CPU magazine.  The magazine lock must be held.
static void
refillMag(struct Slab_Cache *cache, struct perCPU *pc)
{
        assert(pc->magNext == 0);

        TicketLock_Lock(&cache->lock);

        // Fetch entire slabs worth of free objects until we go over
        // the refill limit
        do {
                struct slab *slab = getSlab(cache);

                pc->mag[pc->magNext++] = (void*)slab + slab->freeBuf;

                pc->numFree += slab->numFree;
                slab->numFree = 0;
                slab->freeBuf = 0;
        } while (pc->numFree < MAGAZINE_REFILL);

        TicketLock_Unlock(&cache->lock);
}

// Fetch a partial or empty slab, moving it to the full list.  The
// cache lock must be held.
//
// XXX Could maybe be lock-free.  Depends on free operation.
static struct slab *
getSlab(struct Slab_Cache *cache)
{
        struct slab *slab;
        if (!(slab = slabListTryPop(&cache->partial)) &&
            !(slab = slabListTryPop(&cache->empty))) {
                // XXX Doing this lock-free would be especially nice,
                // though the linked list may trip us up.  OTOH, do we
                // actually need to full list?
                slab = newSlab(cache);
        }
        slabListPush(&cache->full, slab);
        return slab;
}

// Allocate a fresh slab.  Does NOT link it in to the cache.
static struct slab *
newSlab(struct Slab_Cache *cache)
{
        struct slab *slab = Block_Alloc();

        slab->cache = cache;
        slab->freeBuf = cache->bufCtlStart;
        slab->numFree = cache->eltsPerSlab;

        // Free everything
        bufCtl_t *bc = (void*)slab + cache->bufCtlStart;
        void *base = (void*)slab - cache->eltSize;
        for (int i = 0; i < cache->eltsPerSlab - 1;
             ++i, bc = (void*)bc + cache->eltSize)
                // Point this bufctl at the next bufctl.  Really this
                // is (void*)bc - (void*)slab + cache->eltSize, but
                // the latter two never change, so we precompute them.
                *bc = (void*)bc - base;

        return slab;
}

void
Slab_Free(void *obj)
{
        // Free to my local magazine
        struct Slab_Cache *cache = slabOfObj(obj)->cache;
        struct perCPU *pc = &cache->pc[CPU_Current()];
        TicketLock_Lock(&pc->lock);
        if (pc->numFree >= MAGAZINE_SIZE)
                flushMag(cache, pc);
        putMagObj(pc, obj);
        TicketLock_Unlock(&pc->lock);
}

static void
flushMag(struct Slab_Cache *cache, struct perCPU *pc)
{
        TicketLock_Lock(&cache->lock);
        while (pc->numFree >= MAGAZINE_FLUSH)
                // XXX Perhaps we should move entire chains to avoid
                // lots of writes.  But this is really pretty.
                putSlabObj(cache, getMagObj(pc));
        TicketLock_Unlock(&cache->lock);
}

// Add an object to a slab's free list, moving the slab to the partial
// or empty list as appropriate and freeing empty slabs if there are
// too many.  The caller must hold the cache lock and must ensure that
// obj belongs to the given cache.
static void
putSlabObj(struct Slab_Cache *cache, void *obj)
{
        struct slab *slab = slabOfObj(obj);
        bufCtl_t *bc = bufCtlOfObj(obj);
        *bc = slab->freeBuf;
        slab->freeBuf = (void*)bc - (void*)slab;
        slab->numFree++;
        if (slab->numFree == cache->eltsPerSlab) {
                // Move to empty list (note that this case must come
                // first in case eltsPerSlab is 1)
                slabUnlink(slab);
                slabListPush(&cache->empty, slab);
                // XXX Release tail (or this one?) back to block allocator
                //if (++cache->numEmpty > MAX_EMPTY_SLABS) ...
        } else if (slab->numFree == 1) {
                // Move to partial list
                slabUnlink(slab);
                slabListPush(&cache->partial, slab);
        }
}
