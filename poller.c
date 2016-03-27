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

/* Forward declarations for the functions implemented by specific poller
   mechanisms (poll, epoll, kqueue). */
void dill_poller_init(void);
static int dill_poller_addin(int fd);
static void dill_poller_rmin(int fd);
static int dill_poller_addout(int fd);
static void dill_poller_rmout(int fd);
static void dill_poller_clean(int fd);
static int dill_poller_wait(int timeout);

int dill_msleep(int64_t deadline, const char *current) {
    dill_poller_init();
    if(dill_slow(dill_cr_isstopped())) {errno = ECANCELED; return -1;}
    if(dill_slow(deadline == 0)) return 0;
    if(deadline >= 0)
        dill_timer_add(&dill_running->timer, deadline);
    int rc = dill_suspend(NULL);
    if(dill_fast(rc == -ETIMEDOUT)) return 0;
    dill_timer_rm(&dill_running->timer);
    dill_assert(rc < 0);
    errno = -rc;
    return -1;
}

int dill_fdin(int fd, int64_t deadline, const char *current) {
    dill_poller_init();
    if(dill_slow(dill_cr_isstopped())) {errno = ECANCELED; return -1;}
    if(dill_slow(fd < 0)) {errno = EBADF; return -1;}
    int rc = dill_poller_addin(fd);
    if(dill_slow(rc < 0)) return -1;
    if(deadline >= 0)
        dill_timer_add(&dill_running->timer, deadline);
    rc = dill_suspend(NULL);
    /* Handle file descriptor event. */
    if(rc >= 0) {
        if(deadline >= 0)
            dill_timer_rm(&dill_running->timer);
        return 0;
    }
    dill_poller_rmin(fd);
    /* Handle timeout. */
    if(rc == -ETIMEDOUT) {
        errno = ETIMEDOUT;
        return -1;
    }
    if(deadline >= 0)
        dill_timer_rm(&dill_running->timer);
    /* Handle error. */
    errno = -rc;
    return -1;
}

int dill_fdout(int fd, int64_t deadline, const char *current) {
    dill_poller_init();
    if(dill_slow(dill_cr_isstopped())) {errno = ECANCELED; return -1;}
    if(dill_slow(fd < 0)) {errno = EBADF; return -1;}
    int rc = dill_poller_addout(fd);
    if(dill_slow(rc < 0)) return -1;
    if(deadline >= 0)
        dill_timer_add(&dill_running->timer, deadline);
    rc = dill_suspend(NULL);
    /* Handle file descriptor event. */
    if(rc >= 0) {
        if(deadline >= 0)
            dill_timer_rm(&dill_running->timer);
        return 0;
    }
    dill_poller_rmout(fd);
    /* Handle timeout. */
    if(rc == -ETIMEDOUT) {
        errno = ETIMEDOUT;
        return -1;
    }
    if(deadline >= 0)
        dill_timer_rm(&dill_running->timer);
    /* Handle error. */
    errno = -rc;
    return -1;
}

void fdclean(int fd) {
    dill_poller_init();
    dill_poller_clean(fd);
}

void dill_wait(int block) {
    dill_poller_init();
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

