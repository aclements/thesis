#define _GNU_SOURCE             /* RUSAGE_THREAD */

#include "mcorelib.h"

#include <math.h>
#include <sys/time.h>
#include <sys/resource.h>

double
Time_WallSec(void)
{
        struct timeval tv;
        gettimeofday(&tv, 0);
        return tv.tv_sec + tv.tv_usec / 1000000.0;
}

uint64_t
Time_WallUsec(void)
{
        struct timeval tv;
        gettimeofday(&tv, 0);
        return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

uint64_t
Time_SysThreadUsec(void)
{
        struct rusage ru;
        getrusage(RUSAGE_THREAD, &ru);
        return (uint64_t)ru.ru_stime.tv_sec * 1000000 + ru.ru_stime.tv_usec;
}

uint64_t
Time_TSCOverhead(double *stddev_out)
{
        enum { SAMPLES = 512 };
        uint64_t samples[SAMPLES];
        double mean, stddev;

        for (int attempt = 0; attempt < 10; ++attempt) {
                uint64_t sum = 0;
                for (size_t i = 0; i < SAMPLES; ++i) {
                        uint64_t start = Time_TSCBefore();
                        uint64_t end = Time_TSCAfter();
                        samples[i] = end - start;
                        sum += samples[i];
                }

                // Compute stddev
                mean = (double)sum / SAMPLES;
                double variance = 0;
                for (size_t i = 0; i < SAMPLES; ++i)
                        variance += (samples[i] - mean) * (samples[i] - mean);
                variance /= SAMPLES;
                stddev = sqrt(variance);

                if (stddev < 5)
                        break;
        }

        if (stddev_out)
                *stddev_out = stddev;
        uint64_t min = samples[0];
        for (size_t i = 0; i < SAMPLES; ++i)
                if (samples[i] < min)
                        min = samples[i];
        return min;
}
