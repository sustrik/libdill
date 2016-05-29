/*

  Copyright (c) 2016 Martin Sustrik

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

#include "cr.h"
#include "libdill.h"
#include "utils.h"

int64_t now(void) {
#if defined __APPLE__
    static mach_timebase_info_data_t dill_mtid = {0};
    if (dill_slow(!dill_mtid.denom))
        mach_timebase_info(&dill_mtid);
    uint64_t ticks = mach_absolute_time();
    return (int64_t)(ticks * dill_mtid.numer / dill_mtid.denom / 1000000);
#elif defined CLOCK_MONOTONIC
    struct timespec ts;
    int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    dill_assert (rc == 0);
    return ((int64_t)ts.tv_sec) * 1000 + (((int64_t)ts.tv_nsec) / 1000000);
#else
    /* This is slow and error-prone (time can jump backwards!) but it's just
       a last resort option. */
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    assert(rc == 0);
    return ((int64_t)tv.tv_sec) * 1000 + (((int64_t)tv.tv_usec) / 1000);
#endif
}

int msleep(int64_t deadline) {
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Actual waiting. */
    struct dill_tmcl tmcl;
    dill_timer(&tmcl, 1, deadline);
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    return 0;
}

int fdin(int fd, int64_t deadline) {
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Start waiting for the fd. */
    struct dill_clause fdcl;
    rc = dill_in(&fdcl, 1, fd);
    if(dill_slow(rc < 0)) return -1;
    /* Optionally, start waiting for a timer. */
    struct dill_tmcl tmcl;
    dill_timer(&tmcl, 2, deadline);
    /* Block. */
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == 2)) {errno = ETIMEDOUT; return -1;}
    return 0;
}

int fdout(int fd, int64_t deadline) {
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Start waiting for the fd. */
    struct dill_clause fdcl;
    rc = dill_out(&fdcl, 1, fd);
    if(dill_slow(rc < 0)) return -1;
    /* Optionally, start waiting for a timer. */
    struct dill_tmcl tmcl;
    dill_timer(&tmcl, 2, deadline);
    /* Block. */
    int id = dill_wait();
    if(dill_slow(id < 0)) return -1;
    if(dill_slow(id == 2)) {errno = ETIMEDOUT; return -1;}
    return 0;
}

void fdclean(int fd) {
    dill_clean(fd);
}

