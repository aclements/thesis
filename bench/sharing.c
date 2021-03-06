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
#include <sys/mman.h>

enum mode { MODE_WW, MODE_RW, MODE_RR };

struct Args_Choice modeChoices[] = {
        {"ww", MODE_WW}, {"rw", MODE_RW}, {"rr", MODE_RR}, {NULL}
};

struct
{
        int duration;
        int mode;
        int addr_skew;
        int wait;
        CPU_Set_t *cores;
        int write_kde_limit;
        int read_kde_limit;
        bool hugetlb;
} opts = {
        .duration = 1,
        .mode = MODE_WW,
        .addr_skew = 0,
        .wait = 200000,
        .cores = NULL,
        .write_kde_limit = 0,
        .read_kde_limit = 0,
        .hugetlb = false,
};

struct Args_Info argsInfo[] = {
        {"duration", ARGS_INT(&opts.duration),
         .varname = "SECS", .help = "Maximum seconds to run for"},
        {"mode", ARGS_CHOICE(&opts.mode, modeChoices),
         .help = "All cores write; core 1 writes and all read; all cores read"},
        {"addr-skew", ARGS_INT(&opts.addr_skew),
         .help = "Bytes to skew each CPU's access by"},
        {"wait", ARGS_INT(&opts.wait),
         .varname = "CYCLES", .help = "Period between operations"},
        {"cores", ARGS_CPUSET(&opts.cores),
         .varname = "SET", .help = "Set of cores to run on"},
        {"write-kde", ARGS_INT(&opts.write_kde_limit),
         .varname = "LIMIT", .help = "If set, generate KDE up to LIMIT cycles"},
        {"read-kde", ARGS_INT(&opts.read_kde_limit),
         .varname = "LIMIT", .help = "If set, generate KDE up to LIMIT cycles"},
        {"hugetlb", ARGS_BOOL(&opts.hugetlb),
         .help = "Put buffer on a huge page"},
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

// Return the range of values that would fall in bin between the
// lowest value (where==0) and the highest value (where==1)
uint64_t
Histogram_Bin2Value(const struct Histogram *h, size_t bin, double where)
{
        return (uint64_t)((bin + where) * h->limit / HISTOGRAM_BINS);
}

uint64_t
Histogram_Percentile(const struct Histogram *h, double pctile)
{
        uint64_t total = h->over;
        for (size_t i = 0; i < HISTOGRAM_BINS; ++i)
                total += h->bins[i];
        uint64_t goal = total * pctile;
        for (size_t i = 0; i < HISTOGRAM_BINS; ++i) {
                if (h->bins[i] > goal) {
                        // Assume values are evenly distributed in the
                        // bin
                        return Histogram_Bin2Value(
                                h, i, (double)goal / h->bins[i]);
                }
                goal -= h->bins[i];
        }
        panic("%g%% percentile falls outside captured histogram\n",
              100 * pctile);
}

uint64_t
Histogram_IQR(const struct Histogram *h)
{
        return Histogram_Percentile(h, 0.75) - Histogram_Percentile(h, 0.25);
}

double
std_normal(double x)
{
        const double factor = 0.3989422804014327; // 1/sqrt(2 * PI)
        return exp(-x * x / 2) * factor;
}

double
std_normal_integral(double l, double h)
{
        // XXX Numerically unstable for small differences in l and h.
        // Use point sampling instead of integration in the caller
        // when bandwidth is large.
        return (erf(h / M_SQRT2) - erf(l / M_SQRT2)) / 2;
}

void
Histogram_ToKDE(const struct Histogram *hist, const struct StreamStats_Uint *s,
                double *xs, double *ys, size_t out_len)
{
        // Compute bandwidth using Silverman's rule of thumb,
        // augmented with Sheather and Jones (1991) to be robust to
        // outliers.
        double h_scale = 1.06 * pow(s->count, -1.0 / 5), h;
        double std = StreamStats_UintStdDev(s);
        uint64_t iqr = Histogram_IQR(hist);
        if (std < iqr / 1.349)
                h = h_scale * std;
        else
                h = h_scale * (iqr / 1.349);

        // Sample KDE
        for (size_t outi = 0; outi < out_len; ++outi) {
                double x1 = (double)(outi * hist->limit) / out_len;
                double x2 = (double)((outi + 1) * hist->limit) / out_len;
                double x = (x1 + x2) / 2;
                double y = 0;

                // Sum over (weighted) input points
                for (size_t i = 0; i < HISTOGRAM_BINS; ++i) {
                        double xi = Histogram_Bin2Value(hist, i, 0.5);

                        // Single point sample of KDE.  For very small
                        // bandwidths, this can miss features.
                        //double arg = (x - xi) / h;
                        //double K = std_normal(arg);

                        // Average over this range of the KDE sample
                        // by integrating.  The actual contribution is
                        // K*h/(x2-x1), but we factor out the
                        // constants to below.
                        double K;
                        if (h == 0) {
                                // Use a delta function at xi
                                if (x1 <= xi && xi < x2)
                                        K = 1;
                                else
                                        K = 0;
                        } else {
                                double low = (x1 - xi) / h;
                                double high = (x2 - xi) / h;
                                K = std_normal_integral(low, high);
                        }

                        y += hist->bins[i] * K;
                }

                // For point samples
                //y /= (s->count * h);

                y /= s->count * (x2 - x1);

                xs[outi] = x;
                ys[outi] = y;
        }
}

pthread_mutex_t accumLock = PTHREAD_MUTEX_INITIALIZER;
struct StreamStats_Uint totalWriteStats, totalReadStats;
struct Histogram totalWriteHist, totalReadHist;

void
doRead(volatile uint64_t *pos)
{
        uint64_t result;
        asm volatile("mov %1, %0"
                     : "=g" (result)
                     : "m" (*pos));
}

void
doWrite(volatile uint64_t *pos)
{
        __sync_fetch_and_add(pos, 1);
}

int
doOps(int cpu, void *opaque)
{
        struct StreamStats_Uint stats = {};
        struct Histogram hist = {};
        bool reader = false;
        uint64_t myperiod = opts.wait;
        uint64_t myphase = 0;

        switch (opts.mode) {
        case MODE_WW:
                // Make sure CPU 0 always homes the line before the flood
                myperiod *= 2;
                myphase = (cpu > 0) ? myperiod / 2 : 0;
                break;
        case MODE_RW:
                reader = cpu > 0;
                myperiod *= 2;
                myphase = reader ? myperiod / 2 : 0;
                break;
        case MODE_RR:
                reader = true;
                break;
        }

        uint64_t kdeLimit = reader ? opts.read_kde_limit : opts.write_kde_limit;
        if (kdeLimit)
                hist.limit = kdeLimit;

        if (cpu == 0) {
                // Make sure buf is allocated here and faulted
                if (opts.hugetlb) {
                        // This makes a huge difference in
                        // reproducability on ben, I think because it
                        // gives us enough control over the physical
                        // address to control which L3 slice the cache
                        // line falls in.  Offset 0 seems to be best
                        // (but, e.g., offset 128 is ~1500 cycles
                        // slower under high contention!).
                        buf = mmap(0, 2*1024*1024, PROT_READ|PROT_WRITE,
                                   MAP_ANONYMOUS|MAP_PRIVATE|MAP_HUGETLB, -1, 0);
                        if (buf == MAP_FAILED)
                                epanic("mmap buf failed (did you set /proc/sys/vm/nr_hugepages?)");
                } else {
                        buf = numa_alloc_local(4096);
                        if (!buf)
                                epanic("numa_alloc_local failed");
                }

                buf[0] = 0;
        }

        // Start barrier
        if (__sync_sub_and_fetch(&startCount, 1) == 0) {
                startTime = Time_WallSec();
                startFlag = true;
        } else
                while (!startFlag) /**/;

        volatile uint64_t *mybuf = (volatile void*)buf + opts.addr_skew * cpu;
        uint64_t missed = 0;
        while (!stop) {
                uint64_t startTSC = Time_TSCBefore();
                if (reader)
                        doRead(mybuf);
                else
                        doWrite(mybuf);
                uint64_t endTSC = Time_TSCAfter();

                uint64_t deltaTSC = endTSC - startTSC;
                if (deltaTSC < tscOverhead)
                        deltaTSC = 0;
                else
                        deltaTSC -= tscOverhead;
                StreamStats_UintAdd(&stats, deltaTSC);
                if (kdeLimit)
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
                if (kdeLimit)
                        Histogram_Sum(&totalReadHist, &hist);
        } else {
                StreamStats_UintCombine(&totalWriteStats, &stats);
                if (kdeLimit)
                        Histogram_Sum(&totalWriteHist, &hist);
        }
        pthread_mutex_unlock(&accumLock);

        if (0)
                printf("CPU %d: min %"PRIu64" max %"PRIu64" mean %g stddev %g\n",
                       cpu, stats.min, stats.max, StreamStats_UintMean(&stats),
                       StreamStats_UintStdDev(&stats));
        if (missed > 1)
                // We're likely to miss one deadline on the first
                // iteration, so just ignore that.
                fprintf(stderr, "CPU %d missed deadline %"PRIu64" times\n",
                        cpu, missed);

        if (0 && kdeLimit) {
                enum {KDE_SAMPLES = 500};
                double xs[KDE_SAMPLES], ys[KDE_SAMPLES];
                Histogram_ToKDE(&hist, &stats, xs, ys, KDE_SAMPLES);
                char buf[128];
                sprintf(buf, "sharing-kde-%d.data", cpu);
                FILE *kde = fopen(buf, "w");
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

        if (hist->limit) {
                enum {MAX_KDE_SAMPLES = 500};
                double xs[MAX_KDE_SAMPLES], ys[MAX_KDE_SAMPLES];
                size_t kde_samples = MAX_KDE_SAMPLES;
                if (hist->limit < kde_samples)
                        kde_samples = hist->limit;
                Histogram_ToKDE(hist, stats, xs, ys, kde_samples);

                char fname[128];
                sprintf(fname, "sharing-kde-%s.data", op);
                FILE *kde = fopen(fname, "w");
                for (size_t i = 0; i < kde_samples; ++i)
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
        printf("%g cycles/%s\n", StreamStats_UintMean(stats), op);
        printf("%g stddev cycles/%s\n", StreamStats_UintStdDev(stats), op);
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
