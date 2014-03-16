#include "mcorelib.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
panic(const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputs("\n", stderr);
        exit(-1);
}

void
epanic(const char *fmt, ...)
{
        va_list ap;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputs(": ", stderr);
        fputs(strerror(errno), stderr);
        fputs("\n", stderr);
        exit(-1);
}
