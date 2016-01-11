/*

  Copyright (c) 2015 Martin Sustrik

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
#include <sys/param.h>

#include "cr.h"
#include "list.h"
#include "poller.h"
#include "timer.h"
#include "treestack.h"

/* Forward declarations for the functions implemented by specific poller
   mechanisms (poll, epoll, kqueue). */
void ts_poller_init(void);
static void ts_poller_add(int fd, int events);
static void ts_poller_rm(int fd, int events);
static void ts_poller_clean(int fd);
static int ts_poller_wait(int timeout);
static pid_t ts_fork(void);

/* If 1, ts_poller_init was already called. */
static int ts_poller_initialised = 0;

pid_t mfork(void) {
    return ts_fork();
}

/* Pause current coroutine for a specified time interval. */
int ts_msleep(int64_t deadline, const char *current) {
    ts_fdwait(-1, 0, deadline, current);
    return 0;
}

static void ts_poller_callback(struct ts_timer *timer) {
    ts_resume(ts_cont(timer, struct ts_cr, timer), -1);
}

int ts_fdwait(int fd, int events, int64_t deadline, const char *current) {
    if(ts_slow(!ts_poller_initialised)) {
        ts_poller_init();
        ts_assert(errno == 0);
        ts_poller_initialised = 1;
    }
    /* If required, start waiting for the timeout. */
    if(deadline >= 0)
        ts_timer_add(&ts_running->timer, deadline, ts_poller_callback);
    /* If required, start waiting for the file descriptor. */
    if(fd >= 0)
        ts_poller_add(fd, events);
    /* Do actual waiting. */
    ts_running->state = fd < 0 ? TS_MSLEEP : TS_FDWAIT;
    ts_running->fd = fd;
    ts_running->events = events;
    ts_set_current(&ts_running->debug, current);
    int rc = ts_suspend();
    /* Handle file descriptor events. */
    if(rc >= 0) {
        if(deadline >= 0)
            ts_timer_rm(&ts_running->timer);
        return rc;
    }
    /* Handle the timeout. Clean-up the pollset. */
    if(fd >= 0)
        ts_poller_rm(fd, events);
    return 0;
}

void fdclean(int fd) {
    if(ts_slow(!ts_poller_initialised)) {
        ts_poller_init();
        ts_assert(errno == 0);
        ts_poller_initialised = 1;
    }
    ts_poller_clean(fd);
}

void ts_wait(int block) {
    if(ts_slow(!ts_poller_initialised)) {
        ts_poller_init();
        ts_assert(errno == 0);
        ts_poller_initialised = 1;
    }
    while(1) {
        /* Compute timeout for the subsequent poll. */
        int timeout = block ? ts_timer_next() : 0;
        /* Wait for events. */
        int fd_fired = ts_poller_wait(timeout);
        /* Fire all expired timers. */
        int timer_fired = ts_timer_fire();
        /* Never retry the poll in non-blocking mode. */
        if(!block || fd_fired || timer_fired)
            break;
        /* If timeout was hit but there were no expired timers do the poll
           again. This should not happen in theory but let's be ready for the
           case when the system timers are not precise. */
    }
}

/* Include the poll-mechanism-specific stuff. */

/* User overloads. */
#if defined TS_EPOLL
#include "epoll.inc"
#elif defined TS_KQUEUE
#include "kqueue.inc"
#elif defined TS_POLL
#include "poll.inc"
/* Defaults. */
#elif defined __linux__ && !defined TS_NO_EPOLL
#include "epoll.inc"
#elif defined BSD && !defined TS_NO_KQUEUE
#include "kqueue.inc"
#else
#include "poll.inc"
#endif

