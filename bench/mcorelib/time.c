#define _GNU_SOURCE             /* RUSAGE_THREAD */

#include "mcorelib.h"

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
