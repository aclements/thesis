#define _GNU_SOURCE             /* CPU_ZERO, CPU_SET */

#include "mcorelib.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <numa.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void
CPU_Bind(int cpu)
{
        CPU_Set_t *cs = numa_allocate_cpumask();
        numa_bitmask_setbit(cs, cpu);
        int res = numa_sched_setaffinity(0, cs);
        if (res < 0)
                epanic("bindToCPU(%d)", cpu);
        CPU_FreeSet(cs);
}

int
CPU_Current(void)
{
        return sched_getcpu();
}

void
CPU_FreeSet(CPU_Set_t *cs)
{
        numa_bitmask_free(cs);
}

static int
cpuSetMax(CPU_Set_t *cs)
{
        int bit, bits = numa_bitmask_nbytes(cs) * 8;
        for (bit = bits - 1; bit >= 0; bit--)
                if (numa_bitmask_isbitset(cs, bit))
                        break;
        return bit + 1;
}

int
CPU_GetSetMax(CPU_Set_t *cs)
{
        return cpuSetMax(cs);
}

// XXX Should be CPU_GetSetCount or maybe CPUSet_Count
int
CPU_GetCount(CPU_Set_t *cs)
{
        int bit, bits = numa_bitmask_nbytes(cs) * 8, count = 0;
        for (bit = 0; bit <= bits; bit++)
                if (numa_bitmask_isbitset(cs, bit))
                        count++;
        return count;
}

CPU_Set_t *
CPU_ParseSet(const char *s)
{
        // XXX numa_parse_cpustring will only accept CPU's that are
        // less than the highest CPU we're affinitized to and it
        // always masks it to the CPU's the program started with.
//        return numa_parse_cpustring((char*)s);

        struct bitmask *bm = NULL;
        int *cpus = NULL;
        int curCpu = 0, maxCpus = 0, maxCpuVal = 0;
        while (*s) {
                while (isspace(*s)) ++s;
                if (!*s)
                        break;
                if (!isdigit(*s)) {
                        fprintf(stderr, "CPU set expected number: %s", s);
                        goto fail;
                }
                char *end;
                int lo = strtol(s, &end, 10);
                int hi = lo;
                s = end;
                while (isspace(*s)) ++s;
                if (*s == '-') {
                        s++;
                        while (isspace(*s)) ++s;
                        if (!isdigit(*s)) {
                                fprintf(stderr, "CPU set expected number: %s", s);
                                goto fail;
                        }
                        hi = strtol(s, &end, 10);
                        s = end;
                        while (isspace(*s)) ++s;
                }
                for (int cpu = lo; cpu <= hi; ++cpu) {
                        if (curCpu == maxCpus) {
                                maxCpus = maxCpus ? 2*maxCpus : 16;
                                cpus = realloc(cpus, maxCpus * sizeof *cpus);
                        }
                        cpus[curCpu++] = cpu;
                        if (cpu > maxCpuVal)
                                maxCpuVal = cpu;
                }
                if (*s == ',') {
                        s++;
                        continue;
                }
                if (*s) {
                        fprintf(stderr, "CPU set expected ',': %s", s);
                        goto fail;
                }
        }

        bm = numa_bitmask_alloc(maxCpuVal + 1);
        if (!bm)
                panic("Failed to allocate CPU bitmask");
        for (int i = 0; i < curCpu; ++i)
                numa_bitmask_setbit(bm, cpus[i]);

fail:
        free(cpus);
        return bm;
}

CPU_Set_t *
CPU_GetPossible(void)
{
        CPU_Set_t *cs = numa_allocate_cpumask();
        copy_bitmask_to_bitmask(numa_all_cpus_ptr, cs);
        return cs;
}

CPU_Set_t *
CPU_GetAffinity(void)
{
        struct bitmask *cs = numa_allocate_cpumask();
        int res = numa_sched_getaffinity(0, cs);
        if (res < 0)
                epanic("numa_sched_getaffinity");
        return cs;
}

int
CPU_MaxPossible(void)
{
        CPU_Set_t *cs = CPU_GetPossible();
        int m = cpuSetMax(cs);
        CPU_FreeSet(cs);
        return m;
}

struct runOnCPU
{
        int (*fn)(int, void*);
        int cpu, i;
        void *opaque;
        int* results;

        pthread_barrier_t *barrier;
};

static void *
runOnCPUThread(void *opaque)
{
        struct runOnCPU *roc = opaque;

        CPU_Bind(roc->cpu);
        pthread_barrier_wait(roc->barrier);
        int res = roc->fn(roc->cpu, roc->opaque);
        if (roc->results)
                roc->results[roc->i] = res;
        return NULL;
}

/**
 * Run fn on each CPU in cs, passing the CPU number and the opaque
 * argument, then join with all threads.  If cs is NULL, fn is run on
 * all affinitized CPU's (all CPU's by default).  If results is
 * non-NULL, it will be pointed to an array of *nResults results.  The
 * caller must free this array.
 */
void
CPU_RunOnSet(int (*fn)(int, void*), void *opaque, CPU_Set_t *cs,
             int **results, int *nResults)
{
        int r;

        CPU_Set_t *aff = NULL;
        if (!cs) {
                aff = CPU_GetAffinity();
                cs = aff;
        }

        int count = CPU_GetCount(cs);
        int cpus = cpuSetMax(cs);
        struct runOnCPU roc[cpus];
        pthread_t threads[cpus];
        pthread_barrier_t barrier;
        r = pthread_barrier_init(&barrier, NULL, count + 1);
        if (r != 0) {
                errno = r;
                epanic("failed to create pthread barrier");
        }

        if (results)
                *results = malloc(count * sizeof(*results));
        if (nResults)
                *nResults = count;

        // XXX This serializes thread start-up
        int i = 0;
        for (int cpu = 0; cpu < cpus; ++cpu) {
                if (numa_bitmask_isbitset(cs, cpu)) {
                        roc[cpu].fn = fn;
                        roc[cpu].cpu = cpu;
                        roc[cpu].i = i++;
                        roc[cpu].results = results ? *results : NULL;
                        roc[cpu].barrier = &barrier;
                        r = pthread_create(&threads[cpu], NULL, runOnCPUThread,
                                           &roc[cpu]);
                        if (r != 0) {
                                errno = r;
                                epanic("failed to spawn thread on CPU %d", cpu);
                        }
                }
        }

        pthread_barrier_wait(&barrier);

        for (int cpu = 0; cpu < cpus; ++cpu) {
                if (numa_bitmask_isbitset(cs, cpu)) {
                        r = pthread_join(threads[cpu], NULL);
                        if (r != 0) {
                                errno = r;
                                epanic("failed to join thread on CPU %d", cpu);
                        }
                }
        }

        pthread_barrier_destroy(&barrier);
        if (aff)
                CPU_FreeSet(aff);
}

static bool
ArgsCPUSetParse(const struct Args_Info *arg, const char *str)
{
        CPU_Set_t *cs = CPU_ParseSet(str);
        if (!cs) {
                fprintf(stderr, "Option '%s' must be a CPU mask\n", arg->name);
                return false;
        }
        CPU_Set_t **out = arg->dest.dest;
        if (*out)
                CPU_FreeSet(*out);
        *out = cs;
        return true;
}

static char *
ArgsCPUSetShow(const struct Args_Info *arg)
{
        CPU_Set_t *cs = *(CPU_Set_t**)arg->dest.dest;
        if (!cs)
                cs = CPU_GetAffinity();
        int max = cpuSetMax(cs);
        char maxStr[32];
        sprintf(maxStr, "%d,", max);
        char *res = malloc(max * strlen(maxStr) + 1);
        char *pos = res;
        for (int cpu = 0; cpu < max; ++cpu) {
                if (numa_bitmask_isbitset(cs, cpu)) {
                        int end;
                        for (end = cpu + 1; end < max; ++end)
                                if (!numa_bitmask_isbitset(cs, end))
                                        break;
                        if (end == cpu + 1)
                                pos += sprintf(pos, "%d,", cpu);
                        else
                                pos += sprintf(pos, "%d-%d,", cpu, end - 1);
                        cpu = end - 1;
                }
        }
        if (pos != res)
                *(pos - 1) = '\0';
        if (!arg->dest.dest)
                free(cs);
        return res;
}

struct Args_Type Args_TypeCPUSet = {ArgsCPUSetParse, ArgsCPUSetShow,
                                    ARGS_ARGUMENT_REQUIRED};
