#define __STDC_FORMAT_MACROS

#include "mcorelib/mcorelib.h"
#include "mcorelib/args.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <numa.h>

struct
{
        int duration;
        bool rw;
        int wait;
        CPU_Set_t *cores;
        int kde_limit;
} opts = {
        .duration = 1,
        .rw = false,
        .wait = 100000,
        .cores = NULL,
        .kde_limit = 0,
};

struct Args_Info argsInfo[] = {
        {"duration", ARGS_INT(&opts.duration),
         .varname = "SECS", .help = "Maximum seconds to run for"},
        {"rw", ARGS_BOOL(&opts.rw),
         .help = "One core writes, rest read; measure read ops"},
        {"wait", ARGS_INT(&opts.wait),
         .varname = "CYCLES", .help = "Period between operations"},
        {"cores", ARGS_CPUSET(&opts.cores),
         .varname = "SET", .help = "Set of cores to run on"},
        {"kde", ARGS_INT(&opts.kde_limit),
         .varname = "LIMIT", .help = "If set, generate KDE up to LIMIT cycles"},
        {NULL}
};

uint64_t tscOverhead;

int startCount, endCount;
double startTime, endTime;
volatile bool startFlag;
volatile int stop;

volatile uint64_t *buf;

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

enum {HISTOGRAM_BINS = 1024};

struct Histogram
{
        uint64_t bins[HISTOGRAM_BINS];
        uint64_t limit, over;
};

void
Histogram_Add(struct Histogram *h, uint64_t x)
{
        size_t bin = x * HISTOGRAM_BINS / h->limit;
        if (bin < HISTOGRAM_BINS)
                ++h->bins[bin];
        else
                ++h->over;
}

void
Histogram_Sum(struct Histogram *out, const struct Histogram *hist)
{
        if (out->limit == 0)
                out->limit = hist->limit;
        assert(out->limit == hist->limit);
        for (size_t i = 0; i < HISTOGRAM_BINS; ++i)
                out->bins[i] += hist->bins[i];
        out->over += hist->over;
}

void
Histogram_ToKDE(const struct Histogram *hist, const struct StreamStats_Uint *s,
                double *xs, double *ys, size_t out_len)
{
        // Compute bandwidth using Silverman's rule of thumb
        double h = 1.06 * StreamStats_UintStdDev(s) * pow(s->count, -1.0 / 5);

        // Sample KDE
        for (size_t outi = 0; outi < out_len; ++outi) {
                double x = (double)(outi * hist->limit) / out_len;
                double y = 0;

                for (size_t i = 0; i < HISTOGRAM_BINS; ++i) {
                        double xi = (double)(i * hist->limit) / HISTOGRAM_BINS;
                        double arg = (x - xi) / h;
                        double factor = 0.3989422804014327; // 1/sqrt(2 * PI)
                        double K = exp(-arg * arg / 2) * factor;
                        y += hist->bins[i] * K;
                }

                y /= (s->count * h);
                xs[outi] = x;
                ys[outi] = y;
        }
}

pthread_mutex_t accumLock = PTHREAD_MUTEX_INITIALIZER;
struct StreamStats_Uint totalWriteStats, totalReadStats;
struct Histogram totalWriteHist, totalReadHist;

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
        struct StreamStats_Uint stats = {};
        struct Histogram hist = {};
        bool reader = opts.rw && (cpu > 0);

        uint64_t myperiod = opts.rw ? opts.wait * 2 : opts.wait;
        uint64_t myphase = reader ? myperiod / 2 : myperiod;

        if (opts.kde_limit)
                hist.limit = opts.kde_limit;

        if (cpu == 0)
                // Make sure buf is faulted
                buf[0] = 0;

        // Start barrier
        if (__sync_sub_and_fetch(&startCount, 1) == 0) {
                startTime = Time_WallSec();
                startFlag = true;
        } else
                while (!startFlag) /**/;

        uint64_t missed = 0;
        while (!stop) {
                uint64_t startTSC = Time_TSCBefore();
                if (reader)
                        doRead();
                else
                        doWrite();
                uint64_t endTSC = Time_TSCAfter();

                uint64_t deltaTSC = endTSC - startTSC;
                if (deltaTSC < tscOverhead)
                        deltaTSC = 0;
                else
                        deltaTSC -= tscOverhead;
                StreamStats_UintAdd(&stats, deltaTSC);
                if (opts.kde_limit)
                        Histogram_Add(&hist, deltaTSC);

                bool good = false;
                while ((Time_TSC() - myphase) / myperiod ==
                       (startTSC - myphase) / myperiod)
                        good = true;
                missed += !good;
        }

        // End tracking
        if (__sync_sub_and_fetch(&endCount, 1) == 0)
                endTime = Time_WallSec();

        pthread_mutex_lock(&accumLock);
        if (reader) {
                StreamStats_UintCombine(&totalReadStats, &stats);
                if (opts.kde_limit)
                        Histogram_Sum(&totalReadHist, &hist);
        } else {
                StreamStats_UintCombine(&totalWriteStats, &stats);
                if (opts.kde_limit)
                        Histogram_Sum(&totalWriteHist, &hist);
        }
        pthread_mutex_unlock(&accumLock);

        if (0)
                printf("CPU %d: min %"PRIu64" max %"PRIu64" mean %g stddev %g\n",
                       cpu, stats.min, stats.max, StreamStats_UintMean(&stats),
                       StreamStats_UintStdDev(&stats));
        if (missed)
                fprintf(stderr, "CPU %d missed deadline %"PRIu64" times\n",
                        cpu, missed);

        if (0 && opts.kde_limit) {
                enum {KDE_SAMPLES = 500};
                double xs[KDE_SAMPLES], ys[KDE_SAMPLES];
                Histogram_ToKDE(&hist, &stats, xs, ys, KDE_SAMPLES);
                char buf[128];
                sprintf(buf, "write-sharing-kde-%d.data", cpu);
                FILE *kde = fopen(buf, "a");
                for (size_t i = 0; i < KDE_SAMPLES; ++i)
                        fprintf(kde, "%d %g %g\n", CPU_GetCount(opts.cores),
                                xs[i], ys[i]);
                fprintf(kde, "\n");
                fclose(kde);
        }

        return 0;
}

void*
timerThread(void *a)
{
        sleep(opts.duration);
        if (!stop)
                stop = 1;
        return NULL;
}

void
showStats(const char *op, const struct StreamStats_Uint *stats,
          const struct Histogram *hist)
{
        uint64_t ops = stats->count;

        if (!ops)
                return;

        if (opts.kde_limit) {
                enum {KDE_SAMPLES = 500};
                double xs[KDE_SAMPLES], ys[KDE_SAMPLES];
                Histogram_ToKDE(hist, stats, xs, ys, KDE_SAMPLES);

                char fname[128];
                sprintf(fname, "write-sharing-kde-%s.data", op);
                FILE *kde = fopen(fname, "a");
                for (size_t i = 0; i < KDE_SAMPLES; ++i)
                        fprintf(kde, "%d %g %g\n", CPU_GetCount(opts.cores),
                                xs[i], ys[i]);
                fprintf(kde, "\n");
                fclose(kde);

                // Warn if there is significant weight outside of the
                // histogram
                double pct_over = hist->over * 100 / (double)(stats->count);
                if (pct_over >= 1)
                        fprintf(stderr, "Warning: %g%% of %s samples are outside histogram range\n", pct_over, op);
        }

        double start = startTime, end = endTime;

        printf("%"PRIu64" %ss\n", ops, op);
        printf("%"PRIu64" %ss/sec\n", (uint64_t)((double)(ops)/(end-start)), op);
        printf("%"PRIu64" cycles/%s\n",
               (uint64_t)StreamStats_UintMean(stats), op);
        printf("%g stddev cycles/%s\n",
               StreamStats_UintStdDev(stats), op);
}

int
main(int argc, char **argv)
{
        opts.cores = CPU_GetAffinity();
        int ind = Args_Parse(argsInfo, argc, argv);
        if (ind < argc) {
                fprintf(stderr, "Unexpected argument '%s'\n", argv[ind]);
                exit(2);
        }

        char *sum = Args_Summarize(argsInfo);
        printf("# %s\n", sum);
        free(sum);

        buf = numa_alloc_onnode(4096, numa_node_of_cpu(0));
        if (!buf)
                epanic("numa_alloc_onnode failed");

        double tscStddev;
        tscOverhead = Time_TSCOverhead(&tscStddev);
        if (tscStddev >= 5)
                panic("TSC overhead standard deviation of %g too high",
                      tscStddev);

        pthread_t timer;
        pthread_create(&timer, NULL, timerThread, NULL);

        startCount = endCount = CPU_GetCount(opts.cores);
        CPU_RunOnSet(doOps, NULL, opts.cores, NULL, NULL);

        printf("%g sec\n", endTime - startTime);
        showStats("read", &totalReadStats, &totalReadHist);
        showStats("write", &totalWriteStats, &totalWriteHist);
        return 0;
}
