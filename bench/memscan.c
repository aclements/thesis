// Benchmark to scan memory from a set of cores with various
// operations.  The size of the scan can be changed and the memory can
// be shared or per-core.

#define __STDC_FORMAT_MACROS

#include "mcorelib/mcorelib.h"
#include "mcorelib/args.h"

#include <inttypes.h>
#include <numa.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum policy {
        NODE0,
        LOCAL,
        INTERLEAVED
};

struct Args_Choice policyChoices[] = {
        {"node0", NODE0},
        {"local", LOCAL},
        {"interleaved", INTERLEAVED},
        {NULL}
};

enum operation {
        READ,
        WRITE,
        ATOMIC_INC
};

struct Args_Choice opChoices[] = {
        {"read", READ},
        {"write", WRITE},
        {"atomic-inc", ATOMIC_INC},
};

struct
{
        int duration;
        int wait;
        size_t size;
        int op;
        bool dependent;
        bool random;
        bool shared;
        int numa;
        CPU_Set_t *cores;
} opts = {
        .duration = 1,
        .wait = 0,
        .size = 1,
        .dependent = false,
        .random = false,
        .shared = true,
        .numa = NODE0,
        .cores = NULL,
};

struct Args_Info argsInfo[] = {
        {"duration", ARGS_INT(&opts.duration),
         .varname = "SECS", .help = "Maximum seconds to run for."},
        {"wait", ARGS_INT(&opts.wait),
         .varname = "CYCLES", .help = "Cycles to wait between writes"},
        {"size", ARGS_SIZE(&opts.size),
         .varname = "BYTES", .help = "Working set size."},
        {"op", ARGS_CHOICE(&opts.op, opChoices),
         .varname = "OP", .help = "Operation to perform"},
        {"dependent", ARGS_BOOL(&opts.dependent),
         .help = "Make each read dependent on the previous."},
        {"random", ARGS_BOOL(&opts.random),
         .help = "Use a random access pattern."},
        {"shared", ARGS_BOOL(&opts.shared),
         .help = "True: access the same memory; false: per-core memory"},
        {"numa", ARGS_CHOICE(&opts.numa, policyChoices),
         .varname = "POLICY", .help = "NUMA allocation policy"},
        {"cores", ARGS_CPUSET(&opts.cores),
         .varname = "SET", .help = "Set of cores to run on."},
        {NULL}
};

uint64_t tscOverhead;

int startCount;
double startTime;
volatile bool startFlag;
volatile int stop;

struct link
{
        struct link *next;
        int index;
        char _pad[64 - sizeof(struct link*) - sizeof(int)];
};
struct link *buf;
size_t percpu_buf_size;

uint64_t totalOps, totalOpCycles;

struct link *
fillBuffer(int cpu)
{
        struct link *mybuf = buf + cpu * opts.size / sizeof(*buf);
        struct link *mybufend = mybuf + percpu_buf_size;

        if (!opts.random) {
                // Linear scan
                for (struct link *p = mybuf; p < mybufend; p++)
                        p->next = p + 1;
                (mybufend-1)->next = NULL;
        } else {
                // Shuffle by generating a maximum length, cycle-free chain
                mybuf[0].index = 0;
                for (int i = 1; i < percpu_buf_size; i++) {
                        int j = Rand() % (i+1);
                        mybuf[i].index = mybuf[j].index;
                        mybuf[j].index = i;
                }
                for (struct link *p = mybuf; p < mybufend; p++) {
                        struct link *q = p+1;
                        if (q == mybufend) q = mybuf;
                        if (q->index)
                                mybuf[p->index].next = &mybuf[q->index];
                        else
                                mybuf[p->index].next = NULL;
                }
//                for (struct link *p = mybuf; p < mybufend; p++)
//                        printf("%p -> %p\n", p, p->next);
        }
        return mybuf;
}

struct link*
opRead(struct link *p)
{
        struct link *result;
        asm volatile("mov %1, %0"
                     : "=g" (result)
                     : "m" (*p));
        return result;
}

struct link*
opWrite(struct link *p)
{
        p->index++;
        return p->next;
}

struct link*
opAtomicInc(struct link *p)
{
        __sync_fetch_and_add(&p->index, 1);
        return p->next;
}

#define REPEAT(code)                                    \
        for (i = 0; i < repeat && !stop; i++) code

#define DO_OP(OP)                                                       \
        if (opts.dependent) {                                           \
                REPEAT(for (struct link *p = mybuf; p; p = OP(p)));     \
        } else if (opts.random) {                                       \
                REPEAT(for (struct link *p = mybuf; p < mybufend; p++)  \
                               OP(mybuf + Rand() % percpu_buf_size));   \
        } else {                                                        \
                REPEAT(for (struct link *p = mybuf; p < mybufend; p++)  \
                               OP(p));                                  \
        }

int
doOps(int cpu, void *opaque)
{
        int repeat = 1;
        if (opts.wait == 0)
                repeat = 1000;

        struct link *mybuf = opts.shared ? buf : fillBuffer(cpu);
        struct link *mybufend = mybuf + percpu_buf_size;

        // Pre-fetch
        for (struct link *p = mybuf; p; p = p->next);

        // Start barrier
        if (__sync_sub_and_fetch(&startCount, 1) == 0) {
                startTime = Time_WallSec();
                startFlag = true;
        } else
                while (!startFlag) /**/;

        uint64_t ops = 0, opCycles = 0;

        while (!stop) {
                uint64_t startTSC = Time_TSCBefore();
                int i;
                // Why this craziness?  We want the operation inside
                // the repeat loop and the logic to choose an
                // operation outside of the repeat loop.
                switch ((enum operation)opts.op) {
                case READ:
                        DO_OP(opRead);
                        break;
                case WRITE:
                        DO_OP(opWrite);
                        break;
                case ATOMIC_INC:
                        DO_OP(opAtomicInc);
                        break;
                default:
                        panic("Bad operation mode");
                }

                uint64_t endTSC = Time_TSCAfter();
                ops += i * ((opts.size + 63) / 64);
                uint64_t deltaTSC = endTSC - startTSC;
                if (deltaTSC < tscOverhead)
                        deltaTSC = 0;
                else
                        deltaTSC -= tscOverhead;
                opCycles += deltaTSC;
                if (opts.wait) {
                        uint64_t waitTSC;
                        waitTSC = endTSC + opts.wait;
                        while (Time_TSC() < waitTSC);
                }
        }

        __sync_fetch_and_add(&totalOps, ops);
        __sync_fetch_and_add(&totalOpCycles, opCycles);
        return 0;
}

void*
timerThread(void *a)
{
        while (!startFlag) /**/;
        sleep(opts.duration);
        if (!stop)
                stop = 1;
        return NULL;
}

int main(int argc, char **argv)
{
        opts.cores = CPU_GetAffinity();
        int ind = Args_Parse(argsInfo, argc, argv);
        if (ind < argc) {
                fprintf(stderr, "Unexpected argument '%s'\n", argv[ind]);
                exit(2);
        }
        opts.size = ((opts.size - 1) | 63) + 1;

        char *sum = Args_Summarize(argsInfo);
        printf("# %s\n", sum);
        free(sum);

        percpu_buf_size = opts.size / sizeof(*buf);

        size_t bufbytes = opts.size;
        if (!opts.shared)
                bufbytes *= CPU_GetSetMax(opts.cores);
        if (opts.numa == NODE0)
                buf = numa_alloc_onnode(bufbytes, 0);
        else if (opts.numa == LOCAL)
                buf = numa_alloc(bufbytes);
        else if (opts.numa == INTERLEAVED)
                buf = numa_alloc_interleaved(bufbytes);
        if (opts.shared)
                fillBuffer(0);

        double tscStddev;
        tscOverhead = Time_TSCOverhead(&tscStddev);
        if (tscStddev >= 5)
                panic("TSC overhead standard deviation of %g too high",
                      tscStddev);

        pthread_t timer;
        pthread_create(&timer, NULL, timerThread, NULL);

        startCount = CPU_GetCount(opts.cores);
        CPU_RunOnSet(doOps, NULL, opts.cores, NULL, NULL);
        double end = Time_WallSec();
        double start = startTime;
        uint64_t ops = totalOps;

        printf("%g sec\n", end - start);
        printf("%"PRIu64" ops\n", ops);
        printf("%"PRIu64" ops/sec\n", (uint64_t)((double)(ops)/(end-start)));
        if (ops)
                printf("%"PRIu64" cycles/op\n", totalOpCycles/ops);
        return 0;
}

