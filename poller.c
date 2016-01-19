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
#include <sys/param.h>

#include "cr.h"
#include "libdill.h"
#include "list.h"
#include "poller.h"
#include "timer.h"

DILL_CT_ASSERT(sizeof(struct dill_fdwaitdata) <= DILL_OPAQUE_SIZE);

/* Forward declarations for the functions implemented by specific poller
   mechanisms (poll, epoll, kqueue). */
void dill_poller_init(void);
static void dill_poller_add(int fd, int events);
static void dill_poller_rm(int fd, int events);
static void dill_poller_clean(int fd);
static int dill_poller_wait(int timeout);
static pid_t dill_fork(void);

/* If 1, dill_poller_init was already called. */
static int dill_poller_initialised = 0;

pid_t mfork(void) {
    return dill_fork();
}

static int dill_fdwait_(int fd, int events, int64_t deadline,
      const char *current) {
    if(dill_slow(!dill_poller_initialised)) {
        dill_poller_init();
        dill_assert(errno == 0);
        dill_poller_initialised = 1;
    }
    struct dill_fdwaitdata *fdata =
        (struct dill_fdwaitdata*)dill_running->opaque;
    /* If required, start waiting for the timeout. */
    if(deadline >= 0)
        dill_timer_add(&dill_running->timer, deadline);
    /* If required, start waiting for the file descriptor. */
    if(fd >= 0)
        dill_poller_add(fd, events);
    /* Do actual waiting. */
    fdata->fd = fd;
    fdata->events = events;
    dill_startop(&dill_running->debug, fd < 0 ? DILL_MSLEEP : DILL_FDWAIT,
        current);
    int rc = dill_suspend(NULL);
    /* Handle file descriptor events. */
    if(rc >= 0) {
        if(deadline >= 0)
            dill_timer_rm(&dill_running->timer);
        return rc;
    }
    /* Clean up the pollset and the timer. */
    if(rc < 0 && fd >= 0)
        dill_poller_rm(fd, events);
    if(deadline >= 0 && rc != -ETIMEDOUT)
        dill_timer_rm(&dill_running->timer);
    if(dill_slow(rc < 0)) {
        errno = -rc;
        return -1;
    }
    return rc;
}

int dill_msleep(int64_t deadline, const char *current) {
    int rc = dill_fdwait_(-1, 0, deadline, current);
    dill_assert(rc == -1);
    if(errno == ETIMEDOUT)
        return 0;
    return -1;
}

int dill_fdwait(int fd, int events, int64_t deadline, const char *current) {
    if(dill_slow(fd < 0 || events < 0))  {
        errno = EINVAL;
        return -1;
    }
    return dill_fdwait_(fd, events, deadline, current);
}

void fdclean(int fd) {
    if(dill_slow(!dill_poller_initialised)) {
        dill_poller_init();
        dill_assert(errno == 0);
        dill_poller_initialised = 1;
    }
    dill_poller_clean(fd);
}

void dill_wait(int block) {
    if(dill_slow(!dill_poller_initialised)) {
        dill_poller_init();
        dill_assert(errno == 0);
        dill_poller_initialised = 1;
    }
    while(1) {
        /* Compute timeout for the subsequent poll. */
        int timeout = block ? dill_timer_next() : 0;
        /* Wait for events. */
        int fd_fired = dill_poller_wait(timeout);
        /* Fire all expired timers. */
        int timer_fired = dill_timer_fire();
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
#if defined DILL_EPOLL
#include "epoll.inc"
#elif defined DILL_KQUEUE
#include "kqueue.inc"
#elif defined DILL_POLL
#include "poll.inc"
/* Defaults. */
#elif defined __linux__ && !defined DILL_NO_EPOLL
#include "epoll.inc"
#elif defined BSD && !defined DILL_NO_KQUEUE
#include "kqueue.inc"
#else
#include "poll.inc"
#endif

