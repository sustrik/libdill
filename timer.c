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

#include "cr.h"
#include "libdill.h"
#include "timer.h"
#include "utils.h"

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

void dill_timer_postfork(void) {
    dill_list_init(&dill_timers);
}

