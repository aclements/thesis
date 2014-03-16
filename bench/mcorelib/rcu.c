#define _GNU_SOURCE             /* sched_getcpu */

#include "mcorelib.h"

#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

/******************************************************************
 * Thread ID's
 */

#define MAX_THREADS 128

static __thread int myTID = -1;
static char usedTIDs[MAX_THREADS];

static pthread_key_t tidDestructorKey;
static pthread_once_t tidDestructorOnce = PTHREAD_ONCE_INIT;
static bool tidDestructorSetup;

static void
tidDestructor(void *unused)
{
        if (myTID != -1)
                usedTIDs[myTID] = 0;
}

static void
tidDestructorInit(void)
{
        pthread_key_create(&tidDestructorKey, tidDestructor);
        tidDestructorSetup = true;
}

static int __attribute__((__pure__))
getTID(void)
{
        if (myTID != -1)
                return myTID;

        // We don't have a TID yet.

        // We need to free the TID we allocate when this thread exits.
        // Somehow, there is no good way to do this in pthreads, so we
        // steal the pthreads key mechanism because that lets us
        // register thread-exit destructors.
        pthread_once(&tidDestructorOnce, tidDestructorInit);
        while (!ACCESS_ONCE(tidDestructorSetup));
        // Of course, the destructor doesn't run unless we've set our
        // value to something other than NULL.
        pthread_setspecific(tidDestructorKey, (void*)(uintptr_t)1);

        // Allocate a TID, using our current CPU as a starting point
        // to minimize conflicts on the TID table.  In particular, if
        // all threads are pinned, there will be no conflicts.
#ifdef PURIFY
        // Valgrind doesn't support sched_getcpu.
        int start = 0;
#else
        int start = sched_getcpu();
#endif
        for (int i = start; i < start + MAX_THREADS; ++i) {
                int j = i % MAX_THREADS;
                if (__sync_bool_compare_and_swap(&usedTIDs[j], 0, 1)) {
                        myTID = j;
                        return j;
                }
        }

        panic("Threads exceed MAX_THREADS (%d)", MAX_THREADS);
}

/******************************************************************
 * RCU lists
 */

struct RCU_List
{
        struct RCU_Link *head;
        struct RCU_Link **tail;
};

static void
RCUListInit(struct RCU_List *l)
{
        l->head = NULL;
        l->tail = &l->head;
}

static void
RCUListAppend(struct RCU_List *l, struct RCU_Link *elt)
{
        elt->next = NULL;
        *l->tail = elt;
        l->tail = &elt->next;
}

static void
RCUListConcat(struct RCU_List *l, struct RCU_List *rest)
{
        if (rest->head) {
                *l->tail = rest->head;
                l->tail = rest->tail;
        }
}

static void __attribute__((__unused__))
RCUListDump(struct RCU_List *l)
{
        struct RCU_Link **pp = &l->head, *elt;
        while ((elt = *pp)) {
                printf("%p ", elt);
#if RCU_UNIFIED_STRATA
                printf("(%"PRIu64") ", elt->epoch);
#endif
                pp = &elt->next;
        }
        if (pp != l->tail)
                printf("(BAD TAIL %p)", l->tail);
        printf("\n");
}

/******************************************************************
 * RCU state
 */

typedef uint64_t epoch_t;
#define INF_EPOCH ((uint64_t)~0)
static epoch_t globalEpoch = 0;

// The number of items to free per thread before forcing a flush
#if RCU_UNIFIED_STRATA
#define FREE_PER_THREAD_THRESH 1
#else
#define FREE_PER_THREAD_THRESH 16
#endif

static struct RCU_State
{
        epoch_t readEpoch;
        int readNesting;

        // XXX Use an array?
        struct RCU_List localFree;
        int numFree;

        char __pad[0] __attribute__((aligned(64)));
} rcuState[MAX_THREADS];

// We split lists of delay-freed objects up by epoch and store these
// in what we call "strata".  While there are an infinite number of
// epochs, there are a finite number of strata, so we can only track
// free lists for some number of distinct epochs.  If we cycle through
// more than this many epochs without a successful GC, delay-freed
// elements will start moving into later epochs.  Each strata requires
// only ~32 bytes of memory, so in practice, there's little reason to
// allocate a fair number of them.
#if RCU_UNIFIED_STRATA
#define NUM_STRATA 1
#else
#define NUM_STRATA (8*MAX_THREADS)
#endif

static struct RCU_Stratum
{
        pthread_mutex_t lock;
        epoch_t epoch;
        struct RCU_List free;
} strata[NUM_STRATA];

static int gcLock;
static epoch_t gcLast;

static void __attribute__((__constructor__))
RCUInit(void)
{
        for (int i = 0; i < MAX_THREADS; ++i) {
                rcuState[i].readEpoch = INF_EPOCH;
                RCUListInit(&rcuState[i].localFree);
        }

        for (int i = 0; i < NUM_STRATA; ++i) {
                strata[i].lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
                RCUListInit(&strata[i].free);
        }
}

/******************************************************************
 * RCU operations
 */

#define MAX_LINK_OFFSET 4095

void
RCU_ReadLock(void)
{
        struct RCU_State *s = &rcuState[getTID()];

        if (s->readNesting++)
                return;

        s->readEpoch = globalEpoch;
        // We have to guarantee that the above store makes it to
        // memory (and can be observed by GC) before any following
        // loads happen.  If following loads were able to reorder
        // before the readEpoch assignment, we could observe something
        // without pinning its epoch.  XXX This is unfortunate and
        // Linux's SRCU goes through some synchronize_sched rounds
        // precisely to avoid this.  Perhaps the read epoch number
        // could be monotonic and we could store something in addition
        // to the epoch number (possibly before increasing the epoch
        // number, to guarantee it will be seen before the read epoch
        // is brought up, or even in the epoch number).  But what can
        // GC do if it *doesn't* see this other thing?
        __sync_synchronize();
}

void
RCU_ReadUnlock(void)
{
        struct RCU_State *s = &rcuState[getTID()];

        if (--s->readNesting == 0)
                s->readEpoch = INF_EPOCH;
}

void
RCU_Call(struct RCU_Link *l, void (*func)(struct RCU_Link *))
{
        // Add the element to the local free list
        struct RCU_State *s = &rcuState[getTID()];
        l->func = func;
        l->epoch = 0;
        RCUListAppend(&s->localFree, l);
        if (++s->numFree < FREE_PER_THREAD_THRESH)
                return;

        // We've exceeded our local free threshold
        RCU_Flush();
}

void
RCU_Free(const void *ptr, struct RCU_Link *l)
{
        // Inspired by Linux's kfree_rcu hack
        ptrdiff_t offset = (char*)l - (char*)ptr;
        if ((uintptr_t)offset > MAX_LINK_OFFSET)
                panic("RCU_Link is more than MAX_LINK_OFFSET past ptr");
        RCU_Call(l,  (void(*)(struct RCU_Link*))offset);
}

static void
RCU_Invoke(struct RCU_Link *l)
{
        if ((uintptr_t)l->func <= MAX_LINK_OFFSET)
                free(l - (ptrdiff_t)l->func);
        else
                l->func(l);
}

static void
RCU_GC(void) {
        // We only need one GC at a time
        if (__sync_lock_test_and_set(&gcLock, 1))
                return;

        // Find the oldest epoch with a running reader
        //
        // XXX It's unfortunate that this is proportional to the
        // number of threads, and not the number of CPU's.
        epoch_t minEpoch = globalEpoch;
        for (int i = 0; i < MAX_THREADS; ++i) {
                int readEpoch = ACCESS_ONCE(rcuState[i].readEpoch);
                if (readEpoch < minEpoch)
                        minEpoch = readEpoch;
        }

        // Even if a reader fetches an older globalEpoch at this
        // point, it's okay: because of the barrier in RCU_ReadLock,
        // they won't actually observe anything from before minEpoch.

#if RCU_UNIFIED_STRATA
        struct RCU_Stratum *stratum = &strata[0];

        // Collect links to free so we don't hold the lock while
        // invoking callbacks
        pthread_mutex_lock(&stratum->lock);
        struct RCU_Link **pp = &stratum->free.head;
        struct RCU_Link *l, *next;
        struct RCU_List toFree;
        RCUListInit(&toFree);
        while ((l = *pp)) {
                if (l->epoch < minEpoch) {
                        *pp = l->next;
                        RCUListAppend(&toFree, l);
                } else {
                        pp = &l->next;
                }
        }
        stratum->free.tail = pp;
        pthread_mutex_unlock(&stratum->lock);

        // Free them
        for (l = toFree.head; l; l = next) {
                next = l->next;
                RCU_Invoke(l);
        }
#elif RCU_ORDERED
        // Free everything up to minEpoch
        for (epoch_t epoch = gcLast; epoch < minEpoch; ++epoch) {
                struct RCU_Stratum *stratum = &strata[epoch % NUM_STRATA];
                pthread_mutex_lock(&stratum->lock);

                // Unlink what we'll free and release the stratum.  We
                // have to do this before freeing things on the free
                // list so that, if the free function calls back in to
                // RCU, we don't deadlock.
                struct RCU_Link *l = stratum->free.head;
                struct RCU_Link *toFree = l, *next;
                // While the list is ordered by epoch, only the first
                // element in a batch has its epoch set.  The rest are
                // set to 0, so as long as we accept the first element
                // in a batch, we'll accept the rest, too.
                for (; l && l->epoch < minEpoch; l = l->next);
                stratum->free.head = l;
                if (!l)
                        stratum->free.tail = &stratum->free.head;
                pthread_mutex_unlock(&stratum->lock);

                // Free them
                for (; toFree != l; toFree = next) {
                        // Fetch next link before freeing (or in case
                        // func relinks this element).
                        next = toFree->next;
                        RCU_Invoke(toFree);
                }
        }
#else
        // Free everything up to minEpoch
        for (epoch_t epoch = gcLast; epoch < minEpoch; ++epoch) {
                struct RCU_Stratum *stratum = &strata[epoch % NUM_STRATA];
                pthread_mutex_lock(&stratum->lock);
                if (stratum->epoch >= minEpoch) {
                        pthread_mutex_unlock(&stratum->lock);
                        continue;
                }

                // Unlink the free list and release the stratum.  We
                // have to do this before freeing things on the free
                // list so that, if the free function calls back in to
                // RCU, we don't deadlock.
                struct RCU_Link *l = stratum->free.head, *next;
                RCUListInit(&stratum->free);
                pthread_mutex_unlock(&stratum->lock);

                for (; l; l = next) {
                        // Fetch next link before freeing l (or in
                        // case func relinks this element).
                        next = l->next;
                        RCU_Invoke(l);
                }
        }
#endif
        gcLast = minEpoch;

        __sync_lock_release(&gcLock);
}

void
RCU_Flush(void)
{
        struct RCU_State *s = &rcuState[getTID()];

        // Any reader that starts after this point cannot observe any
        // of the objects on the local free list, so we can increment
        // the global epoch.  Unfortunately, we can't just increment
        // it right away: we haven't filled our stratum, so if we
        // increment it now, a racing GC could skip over our stratum.

        epoch_t epoch;
retry:
        epoch = ACCESS_ONCE(globalEpoch);
        struct RCU_Stratum *stratum = &strata[epoch % NUM_STRATA];

        // This lock should have very low contention because writers
        // will naturally stripe themselves across strata
        pthread_mutex_lock(&stratum->lock);
        // Now that we hold a lock on what's hopefully our stratum, we
        // can increment the global epoch.
        if (!__sync_bool_compare_and_swap(&globalEpoch, epoch, epoch + 1)) {
                pthread_mutex_unlock(&stratum->lock);
                goto retry;
        }
#if RCU_UNIFIED_STRATA
        if (s->localFree.head) {
                s->localFree.head->epoch = epoch;
                RCUListConcat(&stratum->free, &s->localFree);
        }
#elif RCU_ORDERED
        if (s->localFree.head) {
                if (stratum->epoch > epoch) {
                        // XXX Insert in sorted order
                        panic("Out-of-order free list not implemented");
                } else {
                        stratum->epoch = epoch;
                        // We only need to set the epoch of the first
                        // element since the rest have been
                        // initialized to 0 in RCU_Call.
                        s->localFree.head->epoch = epoch;
                        RCUListConcat(&stratum->free, &s->localFree);
                }
        }
#else
        stratum->epoch = epoch;
        RCUListConcat(&stratum->free, &s->localFree);
#endif
        pthread_mutex_unlock(&stratum->lock);
        RCUListInit(&s->localFree);
        s->numFree = 0;

        RCU_GC();
}

void
RCU_DumpState(void)
{
        // Freeze up writers
        for (int i = 0; i < NUM_STRATA; ++i)
                pthread_mutex_lock(&strata[i].lock);

        // Per-thread state
        printf("Per-thread:\n");
        for (int i = 0; i < MAX_THREADS; ++i) {
                epoch_t readEpoch = ACCESS_ONCE(rcuState[i].readEpoch);
                int numFree = ACCESS_ONCE(rcuState[i].numFree);
                if (readEpoch == INF_EPOCH && numFree == 0)
                        continue;
                printf("  [%4d] readEpoch ", i);
                if (readEpoch == INF_EPOCH)
                        printf("       INF");
                else
                        printf("%10"PRIu64, readEpoch);
                printf("  numFree %6d\n", numFree);
        }

        // Per-strata state
        printf("Strata:\n");
        for (int i = 0; i < NUM_STRATA; ++i) {
                struct RCU_Link *l;
                int len = 0;
                for (l = strata[i].free.head; l; l = l->next)
                        len++;
                if (len == 0)
                        continue;
                printf("  [%4d] epoch %14"PRIu64"  len %10d\n",
                       i, strata[i].epoch, len);
        }

        // Release writers
        for (int i = 0; i < NUM_STRATA; ++i)
                pthread_mutex_unlock(&strata[i].lock);
}
