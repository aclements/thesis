#define __STDC_FORMAT_MACROS

#include "mcorelib/mcorelib.h"
#include "mcorelib/args.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

struct
{
        int duration;
        bool rw;
        int wait;
        int minwait;
        int readwait;
        CPU_Set_t *cores;
} opts = {
        .duration = 1,
        .rw = false,
        .wait = 10000,
        .minwait = -1,
        .readwait = 10000,
        .cores = NULL,
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
        {NULL}
};

int startCount;
double startTime;
volatile bool startFlag;
volatile int stop;

volatile uint64_t buf[256];

uint64_t totalOps, totalOpCycles;

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
                uint64_t waitTSC;
                if (reader)
                        waitTSC = endTSC + opts.readwait;
                else if (opts.wait == opts.minwait)
                        waitTSC = endTSC + opts.wait;
                else
                        waitTSC = endTSC + (Rand() % (opts.wait - opts.minwait)) + opts.minwait;
                while (Time_TSC() < waitTSC);
        }
        if (reader || !opts.rw) {
                __sync_fetch_and_add(&totalOps, ops);
                __sync_fetch_and_add(&totalOpCycles, opCycles);
        }
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
