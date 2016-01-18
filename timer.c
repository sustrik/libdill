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
static mach_timebase_info_data_t dill_mtid = {0};
#endif

#include "cr.h"
#include "libdill.h"
#include "timer.h"
#include "utils.h"

/* 1 millisecond expressed in CPU ticks. The value is chosen is such a way that
   it works reasonably well for CPU frequencies above 500MHz. On significanly
   slower machines you may wish to reconsider. */
#define DILL_CLOCK_PRECISION 1000000

/* Returns current time by querying the operating system. */
static int64_t dill_now(void) {
#if defined __APPLE__
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
    struct timeval tv;
    int rc = gettimeofday(&tv, NULL);
    assert(rc == 0);
    return ((int64_t)tv.tv_sec) * 1000 + (((int64_t)tv.tv_usec) / 1000);
#endif
}

int64_t now(void) {
#if (defined __GNUC__ || defined __clang__) && \
      (defined __i386__ || defined __x86_64__)
    /* Get the timestamp counter. This is time since startup, expressed in CPU
       cycles. Unlike gettimeofday() or similar function, it's extremely fast -
       it takes only few CPU cycles to evaluate. */
    uint32_t low;
    uint32_t high;
    __asm__ volatile("rdtsc" : "=a" (low), "=d" (high));
    int64_t tsc = (int64_t)((uint64_t)high << 32 | low);
    /* These global variables are used to hold the last seen timestamp counter
       and last seen time measurement. We'll initilise them the first time
       this function is called. */
    static int64_t last_tsc = -1;
    static int64_t last_now = -1;
    if(dill_slow(last_tsc < 0)) {
        last_tsc = tsc;
        last_now = dill_now();
    }   
    /* If TSC haven't jumped back or progressed more than 1/2 ms, we can use
       the cached time value. */
    if(dill_fast(tsc - last_tsc <= (DILL_CLOCK_PRECISION / 2) &&
          tsc >= last_tsc))
        return last_now;
    /* It's more than 1/2 ms since we've last measured the time.
       We'll do a new measurement now. */
    last_tsc = tsc;
    last_now = dill_now();
    return last_now;
#else
    return dill_now();
#endif
}

/* Global linked list of all timers. The list is ordered.
   First timer to be resumed comes first and so on. */
static struct dill_list dill_timers = {0};

void dill_timer_add(struct dill_timer *timer, int64_t deadline) {
    dill_assert(deadline >= 0);
    timer->expiry = deadline;
    /* Move the timer into the right place in the ordered list
       of existing timers. TODO: This is an O(n) operation! */
    struct dill_list_item *it = dill_list_begin(&dill_timers);
    while(it) {
        struct dill_timer *tm = dill_cont(it, struct dill_timer, item);
        /* If multiple timers expire at the same momemt they will be fired
           in the order they were created in (> rather than >=). */
        if(tm->expiry > timer->expiry)
            break;
        it = dill_list_next(it);
    }
    dill_list_insert(&dill_timers, &timer->item, it);
}

void dill_timer_rm(struct dill_timer *timer) {
    dill_list_erase(&dill_timers, &timer->item);
}

int dill_timer_next(void) {
    if(dill_list_empty(&dill_timers))
        return -1;
    int64_t nw = now();
    int64_t expiry = dill_cont(dill_list_begin(&dill_timers),
        struct dill_timer, item)->expiry;
    return (int) (nw >= expiry ? 0 : expiry - nw);
}

int dill_timer_fire(void) {
    /* Avoid getting current time if there are no timers anyway. */
    if(dill_list_empty(&dill_timers))
        return 0;
    int64_t nw = now();
    int fired = 0;
    while(!dill_list_empty(&dill_timers)) {
        struct dill_timer *tm = dill_cont(
            dill_list_begin(&dill_timers), struct dill_timer, item);
        if(tm->expiry > nw)
            break;
        dill_list_erase(&dill_timers, dill_list_begin(&dill_timers));
        dill_resume(dill_cont(tm, struct dill_cr, timer), -ETIMEDOUT);
        fired = 1;
    }
    return fired;
}

