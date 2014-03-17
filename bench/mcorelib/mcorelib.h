#ifndef _MCORELIB_H
#define _MCORELIB_H

#include <stddef.h>
#include <stdint.h>

#include "args.h"
#include "locks.h"

// Macros
#define CACHE_LINE_BYTES 64
#define ACCESS_ONCE(x) (*(volatile __typeof(x) *)&(x))
#define container_of(ptr, type, member)                                 \
        (__extension__                                                  \
         ({                                                             \
                 const __typeof__(((type *)0)->member) *__mptr = (ptr); \
                 (type *)((char *)__mptr - offsetof(type, member));     \
         }))

// msg.c
void panic(const char *fmt, ...)
        __attribute__((__noreturn__, __format__(printf, 1, 2)));
void epanic(const char *fmt, ...)
        __attribute__((__noreturn__, __format__(printf, 1, 2)));

// cpu.c
typedef struct bitmask CPU_Set_t;
void CPU_Bind(int cpu);
int CPU_Current(void);
void CPU_FreeSet(CPU_Set_t *cs);
int CPU_GetSetMax(CPU_Set_t *cs);
int CPU_GetCount(CPU_Set_t *cs);
CPU_Set_t *CPU_ParseSet(const char *s);
CPU_Set_t *CPU_GetPossible(void);
CPU_Set_t *CPU_GetAffinity(void);
int CPU_MaxPossible(void);
void CPU_RunOnSet(int (*fn)(int, void*), void *opaque, CPU_Set_t *mask,
                  int **results, int *nResults);
extern struct Args_Type Args_TypeCPUSet;
#define ARGS_CPUSET(d) .type = &Args_TypeCPUSet, .dest = {.dest = (d)}

// time.c
double Time_WallSec(void);
uint64_t Time_WallUsec(void);
uint64_t Time_SysThreadUsec(void);

static inline uint64_t __attribute__((__always_inline__))
Time_TSC(void)
{
        uint32_t a, d;
        __asm __volatile("rdtsc" : "=a" (a), "=d" (d));
        return ((uint64_t) a) | (((uint64_t) d) << 32);
}

static inline uint64_t __attribute__((__always_inline__))
Time_TSCBefore(void)
{
        // See the "Improved Benchmarking Method" in Intel's "How to
        // Benchmark Code Execution Times on Intel® IA-32 and IA-64
        // Instruction Set Architectures"
        uint32_t a, d;
        __asm __volatile("cpuid; rdtsc; mov %%eax, %0; mov %%edx, %1"
                         : "=r" (a), "=r" (d)
                         : : "%rax", "%rbx", "%rcx", "%rdx");
        return ((uint64_t) a) | (((uint64_t) d) << 32);
}

static inline uint64_t __attribute__((__always_inline__))
Time_TSCAfter(void)
{
        uint32_t a, d;
        __asm __volatile("rdtscp; mov %%eax, %0; mov %%edx, %1; cpuid"
                         : "=r" (a), "=r" (d)
                         : : "%rax", "%rbx", "%rcx", "%rdx");
        return ((uint64_t) a) | (((uint64_t) d) << 32);
}

// block.c
enum { BLOCK_SIZE = 4096 };
void *Block_Alloc(void);
void Block_Free(void *ptr);
static inline void *
Block_Begin(void *ptr)
{
        return (void*)(((uintptr_t)ptr) & ~(BLOCK_SIZE - 1));
}

// slab.c
struct Slab_Cache;

#define Slab_Create(typ) Slab_CreateEx(sizeof(typ), #typ)
struct Slab_Cache *Slab_CreateEx(size_t eltSize, const char *name);
void *Slab_Alloc(struct Slab_Cache *cache);
void Slab_Free(void *obj);

// rand.c
int Rand(void);

// rcu.c
#ifndef RCU_UNIFIED_STRATA
// If 1, use a single free list with epochs recorded in each RCU_Link.
#define RCU_UNIFIED_STRATA 0
#endif
#ifndef RCU_ORDERED
// If 1, prevent roll-around of strata epoch by recording the free
// epoch in each RCU_Link.
#define RCU_ORDERED 1
#endif
struct RCU_Link
{
        struct RCU_Link *next;
        void (*func)(struct RCU_Link *l);
#if RCU_UNIFIED_STRATA || RCU_ORDERED
        uint64_t epoch;
#endif
};
void RCU_ReadLock(void);
void RCU_ReadUnlock(void);
void RCU_Call(struct RCU_Link *l, void (*func)(struct RCU_Link *));
void RCU_Free(const void *ptr, struct RCU_Link *l);
void RCU_Flush(void);
void RCU_DumpState(void);

#endif // _MCORELIB_H
