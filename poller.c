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
#include "fd.h"
#include "list.h"
#include "poller.h"
#include "pollset.h"
#include "utils.h"

/* 1 if dill_poller_init() was already run. */
static int dill_poller_initialised = 0;

/* Global linked list of all timers. The list is ordered.
   First timer to be resumed comes first and so on. */
static struct dill_list dill_timers;

static void dill_poller_init(int parent) {
    /* If intialisation was already done, do nothing. */
    if(dill_fast(dill_poller_initialised)) return;
    dill_poller_initialised = 1;
    /* Timers. */
    dill_list_init(&dill_timers);
    /* Polling-mechanism-specific intitialisation. */
    int rc = dill_pollset_init(parent);
    dill_assert(rc == 0);
}

static void dill_poller_term(void) {
    dill_poller_initialised = 0;
    /* Polling-mechanism-specific termination. */
    dill_pollset_term();
    /* Get rid of all the timers inherited from the parent. */
    dill_list_init(&dill_timers);
}

void dill_poller_postfork(int parent) {
    dill_poller_term();
    dill_poller_init(parent);
}

/* Adds a timer clause to the list of waited for clauses. */
void dill_timer(struct dill_tmcl *tmcl, int id, int64_t deadline) {
    dill_poller_init(-1);
    dill_assert(deadline >= 0);
    tmcl->deadline = deadline;
    /* Move the timer into the right place in the ordered list
       of existing timers. TODO: This is an O(n) operation! */
    struct dill_list_item *it = dill_list_begin(&dill_timers);
    while(it) {
        struct dill_tmcl *itcl = dill_cont(it, struct dill_tmcl, cl.epitem);
        /* If multiple timers expire at the same momemt they will be fired
           in the order they were created in (> rather than >=). */
        if(itcl->deadline > tmcl->deadline)
            break;
        it = dill_list_next(it);
    }
    dill_waitfor(&tmcl->cl, id, &dill_timers, it);
}

/* Returns number of milliseconds till next timer expiration.
   -1 stands for infinity. */
static int dill_timer_next(void) {
    if(dill_list_empty(&dill_timers))
        return -1;
    int64_t nw = now();
    int64_t deadline = dill_cont(dill_list_begin(&dill_timers),
        struct dill_tmcl, cl.epitem)->deadline;
    return (int) (nw >= deadline ? 0 : deadline - nw);
}

int dill_in(struct dill_clause *cl, int id, int fd) {
    dill_poller_init(-1);
    if(dill_slow(fd < 0 || fd >= dill_maxfds())) {errno = EBADF; return -1;}
    return dill_pollset_in(cl, id, fd);
}

int dill_out(struct dill_clause *cl, int id, int fd) {
    dill_poller_init(-1);
    if(dill_slow(fd < 0 || fd >= dill_maxfds())) {errno = EBADF; return -1;}
    return dill_pollset_out(cl, id, fd);
}

void dill_clean(int fd) {
    dill_poller_init(-1);
    dill_pollset_clean(fd);
}

void dill_poller_wait(int block) {
    dill_poller_init(-1);
    while(1) {
        /* Compute timeout for the subsequent poll. */
        int timeout = block ? dill_timer_next() : 0;
        /* Wait for events. */
        int fd_fired = dill_pollset_poll(timeout);
        /* Fire all expired timers. */
        int timer_fired = 0;
        if(!dill_list_empty(&dill_timers)) {
            int64_t nw = now();
            while(!dill_list_empty(&dill_timers)) {
                struct dill_tmcl *tmcl = dill_cont(
                    dill_list_begin(&dill_timers), struct dill_tmcl, cl.epitem);
                if(tmcl->deadline > nw)
                    break;
                dill_list_erase(&dill_timers, dill_list_begin(&dill_timers));
                dill_trigger(&tmcl->cl, ETIMEDOUT);
                timer_fired = 1;
            }
        }
        /* Never retry the poll in non-blocking mode. */
        if(!block || fd_fired || timer_fired)
            break;
        /* If timeout was hit but there were no expired timers do the poll
           again. It can happen if the timers were canceled in the meantime. */
    }
}

