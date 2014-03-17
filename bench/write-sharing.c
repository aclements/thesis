#define __STDC_FORMAT_MACROS

#include "mcorelib/mcorelib.h"
#include "mcorelib/args.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>

// XXX Reads are actually *more* expensive than this benchmark
// indicates.  Fast reads not only bring down the average but also
// allow the next read to occur sooner.  For example, cores that are
// on the same socket as the writer not only make much faster reads
// but also make *far more of them*, artificially reducing the average
// read latency.  I can't just space the reads out more because then
// we don't get the effect of simultaneous reads.  I could depend on
// synchronized TSCs and schedule the reads at regular TSC multiples.
//
// OTOH, reads might be cheaper than this benchmark indicates.  If the
// writer is running really fast, then this won't be 1 write N reads.

struct
{
        int duration;
        bool rw;
        int wait;
        int minwait;
        int readwait;
        CPU_Set_t *cores;
        bool stats;
} opts = {
        .duration = 1,
        .rw = false,
        .wait = 10000,
        .minwait = -1,
        .readwait = 10000,
        .cores = NULL,
        .stats = false,
};

struct Args_Info argsInfo[] = {
        {"duration", ARGS_INT(&opts.duration),
         .varname = "SECS", .help = "Maximum seconds to run for."},
        {"rw", ARGS_BOOL(&opts.rw),
         .help = "One core writes, rest read; measure read ops"},
        {"wait", ARGS_INT(&opts.wait),
         .varname = "CYCLES", .help = "Cycles to wait between writes"},
        {"minwait", ARGS_INT(&opts.minwait),
         .varname = "CYCLES", .help = "If set, minimum cycles to wait"},
        {"readwait", ARGS_INT(&opts.readwait),
         .varname = "CYCLES", .help = "Cycles to wait between reads"},
        {"cores", ARGS_CPUSET(&opts.cores),
         .varname = "SET", .help = "Set of cores to run on."},
        {"stats", ARGS_BOOL(&opts.stats),
         .help = "Report additional per-CPU statistics."},
        {NULL}
};

int startCount;
double startTime;
volatile bool startFlag;
volatile int stop;

volatile uint64_t buf[256];

uint64_t totalOps, totalOpCycles;

struct StreamStats_Uint
{
        uint64_t count;
        uint64_t total, min, max;
        // Online variance
        double vmean, vM2;
};

void
StreamStats_UintAdd(struct StreamStats_Uint *s, uint64_t val)
{
        s->total += val;
        if (s->count == 0) {
                s->min = s->max = val;
        } else {
                if (val < s->min)
                        s->min = val;
                if (val > s->max)
                        s->max = val;
        }
        ++s->count;
        // Based on Wikipedia's presentation ("Algorithms for
        // calculating variance") of Knuth's formulation of Welford
        // 1962.
        double delta = val - s->vmean;
        s->vmean += delta / s->count;
        s->vM2 += delta * (val - s->vmean);
}

double
StreamStats_UintMean(const struct StreamStats_Uint *s)
{
        if (s->total / 10000 > s->count)
                return (double)(s->total / s->count);
        return ((double)s->total) / s->count;
}

double
StreamStats_UintStdDev(const struct StreamStats_Uint *s)
{
        return sqrt(s->vM2 / (s->count - 1));
}

void
StreamStats_UintCombine(struct StreamStats_Uint *a,
                        const struct StreamStats_Uint *b)
{
        double delta = b->vmean - a->vmean;
        uint64_t count = a->count + b->count;
        double vmean = a->vmean + delta * b->count / count;
        double vM2 = a->vM2 + b->vM2 +
                delta * delta * a->count * b->count / count;

        a->count = count;
        a->total += b->total;
        if (b->min < a->min)
                a->min = b->min;
        if (b->max > a->max)
                a->max = b->max;
        a->vmean = vmean;
        a->vM2 = vM2;
}

__attribute__((noinline)) void
doRead()
{
        uint64_t result;
        asm volatile("mov %1, %0"
                     : "=g" (result)
                     : "m" (buf[128]));
}

__attribute__((noinline)) void
doWrite()
{
        /* 
         * __sync_synchronize();
         * buf[128] = 1;
         * __sync_synchronize();
         */
        
        __sync_fetch_and_add(&buf[128], 1);
}

int
doOps(int cpu, void *opaque)
{
        struct StreamStats_Uint ss = {};
        bool reader = opts.rw && (cpu > 0);

        // Start barrier
        if (__sync_sub_and_fetch(&startCount, 1) == 0) {
                startTime = Time_WallSec();
                startFlag = true;
        } else
                while (!startFlag) /**/;

        uint64_t ops = 0, opCycles = 0;
        while (!stop) {
                uint64_t startTSC = Time_TSCBefore();
                if (reader)
                        doRead();
                else
                        doWrite();
                uint64_t endTSC = Time_TSCAfter();
                ops++;
                opCycles += (endTSC - startTSC);
                if (opts.stats)
                        StreamStats_UintAdd(&ss, endTSC - startTSC);
                uint64_t waitTSC;
                if (reader)
                        waitTSC = endTSC + opts.readwait;
                else if (opts.wait == opts.minwait)
                        waitTSC = endTSC + opts.wait;
                else
                        waitTSC = endTSC + (Rand() % (opts.wait - opts.minwait)) + opts.minwait;
                if (endTSC != waitTSC)
                        while (Time_TSC() < waitTSC);
        }
        if (reader || !opts.rw) {
                __sync_fetch_and_add(&totalOps, ops);
                __sync_fetch_and_add(&totalOpCycles, opCycles);
        }
        if (opts.stats)
                printf("CPU %d: min %"PRIu64" max %"PRIu64" mean %g stddev %g\n",
                       cpu, ss.min, ss.max, StreamStats_UintMean(&ss),
                       StreamStats_UintStdDev(&ss));
}

void*
timerThread(void *a)
{
        sleep(opts.duration);
        if (!stop)
                stop = 1;
}

int main(int argc, char **argv)
{
        opts.cores = CPU_GetAffinity();
        int ind = Args_Parse(argsInfo, argc, argv);
        if (ind < argc) {
                fprintf(stderr, "Unexpected argument '%s'\n", argv[ind]);
                exit(2);
        }
        if (opts.minwait == -1)
                opts.minwait = opts.wait;

        char *sum = Args_Summarize(argsInfo);
        printf("# %s\n", sum);
        free(sum);

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
