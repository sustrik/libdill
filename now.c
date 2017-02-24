/*

  Copyright (c) 2017 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

#if defined __APPLE__
#include <mach/mach_time.h>
#endif

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define DILL_RDTSC_DIFF 1000000ULL
#endif

#include "cr.h"
#include "libdill.h"
#include "pollset.h"
#include "utils.h"

static int64_t mnow(void) {
#if defined __APPLE__
    static mach_timebase_info_data_t dill_mtid = {0};
    if (dill_slow(!dill_mtid.denom))
        mach_timebase_info(&dill_mtid);
    uint64_t ticks = mach_absolute_time();
    return (int64_t)(ticks * dill_mtid.numer / dill_mtid.denom / 1000000);
#else

#if defined CLOCK_MONOTONIC_COARSE
    clock_t id = CLOCK_MONOTONIC_COARSE;
#elif defined CLOCK_MONOTONIC_FAST
    clock_t id = CLOCK_MONOTONIC_FAST;
#elif defined CLOCK_MONOTONIC
    clock_t id = CLOCK_MONOTONIC;
#else
#define DILL_NOW_FALLBACK
#endif

#if !defined DILL_NOW_FALLBACK
    struct timespec ts;
    int rc = clock_gettime(id, &ts);
    dill_assert (rc == 0);
    return ((int64_t)ts.tv_sec) * 1000 + (((int64_t)ts.tv_nsec) / 1000000);
#else
    /* This is slow and error-prone (the time can jump backwards!), but it's just
       a last resort option. */
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    dill_assert (rc == 0);
    return ((int64_t)tv.tv_sec) * 1000 + (((int64_t)tv.tv_usec) / 1000);
#endif

#endif
}

int64_t now(void) {
#if defined(__x86_64__) || defined(__i386__)
    static int64_t last_tick = 0;
    static uint64_t last_rdtsc = 0;
    uint64_t rdtsc = __rdtsc();
    int64_t diff = rdtsc - last_rdtsc;
    if(diff < 0) diff = -diff;
    if(dill_fast(diff < DILL_RDTSC_DIFF))
        return last_tick;
    else
        last_rdtsc = rdtsc;
    return (last_tick = mnow());
#else
    return mnow();
#endif
}

