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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#include "cr.h"
#include "utils.h"

#define TS_ENDLIST 0xffffffff

#define TS_EPOLLSETSIZE 128

/* Global pollset. */
static int ts_efd = -1;

/* Epoll allows to register only a single pointer with a file decriptor.
   However, we may need two pointers to coroutines. One for the coroutine
   waiting to receive data from the descriptor, one for the coroutine waiting
   to send data to the descriptor. Thus, we are going to keep an array of
   pointer pairs for each file descriptor. */
struct ts_crpair {
    struct ts_cr *in;
    struct ts_cr *out;
    uint32_t currevs;
    /* 1-based index, 0 stands for "not part of the list", TS_ENDLIST
       stads for "no more elements in the list. */
    uint32_t next;
};

static struct ts_crpair *ts_crpairs = NULL;
static int ts_ncrpairs = 0;
static uint32_t ts_changelist = TS_ENDLIST;

void ts_poller_init(void) {
    struct rlimit rlim;
    int rc = getrlimit(RLIMIT_NOFILE, &rlim);
    if(ts_slow(rc < 0)) return;
    ts_ncrpairs = rlim.rlim_max;
    ts_crpairs = (struct ts_crpair*)
        calloc(ts_ncrpairs, sizeof(struct ts_crpair));
    if(ts_slow(!ts_crpairs)) {errno = ENOMEM; return;}
    ts_efd = epoll_create(1);
    if(ts_slow(ts_efd < 0)) {
        free(ts_crpairs);
        ts_crpairs = NULL;
        return;
    }
    errno = 0;
}

pid_t ts_fork(void) {
    pid_t pid = fork();
    if(pid != 0)
        return pid;
    /* In the child process we have to create a new kernel-side pollset,
       identical to the one in the parent process, yet not sharing its
       destiny. */
    if(ts_efd != -1) {
        int rc = close(ts_efd);
        ts_assert(rc == 0);
        ts_efd = epoll_create(1);
        ts_assert(ts_efd >= 0);
        int fd;
        for(fd = 0; fd != ts_ncrpairs; ++fd) {
            if(ts_crpairs[fd].currevs) {
                struct epoll_event ev;
                ev.data.fd = fd;
                ev.events = 0;
                if(ts_crpairs[fd].currevs & FDW_IN)
                    ev.events |= EPOLLIN;
                if(ts_crpairs[fd].currevs & FDW_OUT)
                    ev.events |= EPOLLOUT;
                rc = epoll_ctl(ts_efd, EPOLL_CTL_ADD, fd, &ev);
                ts_assert(rc == 0);
            }
        }
    }
    errno = 0;
    return 0;
}

static void ts_poller_add(int fd, int events) {
    struct ts_crpair *crp = &ts_crpairs[fd];
    if(events & FDW_IN) {
        if(crp->in)
            ts_panic(
                "multiple coroutines waiting for a single file descriptor");
        crp->in = ts_running;
    }
    if(events & FDW_OUT) {
        if(crp->out)
            ts_panic(
                "multiple coroutines waiting for a single file descriptor");
        crp->out = ts_running;
    }
    if(!crp->next) {
        crp->next = ts_changelist;
        ts_changelist = fd + 1;
    }
}

static void ts_poller_rm(int fd, int events) {
    struct ts_crpair *crp = &ts_crpairs[fd];
    if(events & FDW_IN)
        crp->in = NULL;
    if(events & FDW_OUT)
        crp->out = NULL;
    if(!crp->next) {
        crp->next = ts_changelist;
        ts_changelist = fd + 1;
    }
}

static void ts_poller_clean(int fd) {
    struct ts_crpair *crp = &ts_crpairs[fd];
    ts_assert(!crp->in);
    ts_assert(!crp->out);
    /* Remove the file descriptor from the pollset, if it is still present. */
    if(crp->currevs) {   
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = 0;
        int rc = epoll_ctl(ts_efd, EPOLL_CTL_DEL, fd, &ev);
        ts_assert(rc == 0 || errno == ENOENT);
    }
    /* Clean the cache. */
    crp->currevs = 0;
    if(!crp->next) {
        crp->next = ts_changelist;
        ts_changelist = fd + 1;
    }
}

static int ts_poller_wait(int timeout) {
    /* Apply any changes to the pollset.
       TODO: Use epoll_ctl_batch once available. */
    while(ts_changelist != TS_ENDLIST) {
        int fd = ts_changelist - 1;
        struct ts_crpair *crp = &ts_crpairs[fd];
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = 0;
        if(crp->in)
            ev.events |= EPOLLIN;
        if(crp->out)
            ev.events |= EPOLLOUT;
        if(crp->currevs != ev.events) {
            int op;
            if(!ev.events)
                 op = EPOLL_CTL_DEL;
            else if(!crp->currevs)
                 op = EPOLL_CTL_ADD;
            else
                 op = EPOLL_CTL_MOD;
            crp->currevs = ev.events;
            int rc = epoll_ctl(ts_efd, op, fd, &ev);
            ts_assert(rc == 0);
        }
        ts_changelist = crp->next;
        crp->next = 0;
    }
    /* Wait for events. */
    struct epoll_event evs[TS_EPOLLSETSIZE];
    int numevs;
    while(1) {
        numevs = epoll_wait(ts_efd, evs, TS_EPOLLSETSIZE, timeout);
        if(numevs < 0 && errno == EINTR)
            continue;
        ts_assert(numevs >= 0);
        break;
    }
    /* Fire file descriptor events. */
    int i;
    for(i = 0; i != numevs; ++i) {
        struct ts_crpair *crp = &ts_crpairs[evs[i].data.fd];
        int inevents = 0;
        int outevents = 0;
        /* Set the result values. */
        if(evs[i].events & EPOLLIN)
            inevents |= FDW_IN;
        if(evs[i].events & EPOLLOUT)
            outevents |= FDW_OUT;
        if(evs[i].events & (EPOLLERR | EPOLLHUP)) {
            inevents |= FDW_ERR;
            outevents |= FDW_ERR;
        }
        /* Resume the blocked coroutines. */  
        if(crp->in == crp->out) {
            ts_resume(crp->in, inevents | outevents);
            ts_poller_rm(evs[i].data.fd, FDW_IN | FDW_OUT);
        }
        else {
            if(crp->in && inevents) {
                ts_resume(crp->in, inevents);
                ts_poller_rm(evs[i].data.fd, FDW_IN);
            }
            if(crp->out && outevents) {
                ts_resume(crp->out, outevents);
                ts_poller_rm(evs[i].data.fd, FDW_OUT);
            }
        }
    }
    /* Return 0 in case of time out. 1 if at least one coroutine was resumed. */
    return numevs > 0 ? 1 : 0;
}

