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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/resource.h>

#include "cr.h"
#include "utils.h"

#define DILL_ENDLIST 0xffffffff

#define DILL_EPOLLSETSIZE 128

/* Global pollset. */
static int dill_efd = -1;

/* Epoll allows to register only a single pointer with a file decriptor.
   However, we may need two pointers to coroutines. One for the coroutine
   waiting to receive data from the descriptor, one for the coroutine waiting
   to send data to the descriptor. Thus, we are going to keep an array of
   pointer pairs for each file descriptor. */
struct dill_crpair {
    struct dill_cr *in;
    struct dill_cr *out;
    uint32_t currevs;
    /* 1-based index, 0 stands for "not part of the list", DILL_ENDLIST
       stads for "no more elements in the list. */
    uint32_t next;
};

static struct dill_crpair *dill_crpairs = NULL;
static int dill_ncrpairs = 0;
static uint32_t dill_changelist = DILL_ENDLIST;
static struct dill_crpair *dill_parent = NULL;

static void dill_poller_atexit(void) {
    if(dill_crpairs)
        free(dill_crpairs);
}

void dill_poller_init(void) {
    if(dill_fast(dill_efd >= 0))
        return;
    struct rlimit rlim;
    int rc = getrlimit(RLIMIT_NOFILE, &rlim);
    if(dill_slow(rc < 0)) return;
    dill_ncrpairs = rlim.rlim_max;
    dill_crpairs = (struct dill_crpair*)
        calloc(dill_ncrpairs, sizeof(struct dill_crpair));
    if(dill_slow(!dill_crpairs)) {errno = ENOMEM; return;}
    dill_efd = epoll_create(1);
    if(dill_slow(dill_efd < 0)) {
        free(dill_crpairs);
        dill_crpairs = NULL;
        return;
    }
    rc = atexit(dill_poller_atexit);
    dill_assert(rc == 0);
    errno = 0;
}

void dill_poller_postfork(int parent) {
    if(dill_efd != -1) {
        int rc = close(dill_efd);
        dill_assert(rc == 0);
    }
    dill_efd = -1;
    dill_crpairs = NULL;
    dill_ncrpairs = 0;
    dill_changelist = DILL_ENDLIST;
    dill_poller_init();
    /* Start polling for signals from the parent. */
    dill_parent = &dill_crpairs[parent];
    dill_assert(!dill_parent->in);
    dill_parent->in = dill_running;
    dill_assert(!dill_parent->next);
    dill_parent->next = dill_changelist;
    dill_changelist = parent + 1;
}

static int dill_poller_addin(int fd) {
    struct dill_crpair *crp = &dill_crpairs[fd];
    if(dill_slow(crp->in)) {
        errno = EEXIST;
        return -1;
    }
    crp->in = dill_running;
    if(!crp->next) {
        crp->next = dill_changelist;
        dill_changelist = fd + 1;
    }
    return 0;
}

static int dill_poller_addout(int fd) {
    struct dill_crpair *crp = &dill_crpairs[fd];
    if(dill_slow(crp->out)) {
        errno = EEXIST;
        return -1;
    }
    crp->out = dill_running;
    if(!crp->next) {
        crp->next = dill_changelist;
        dill_changelist = fd + 1;
    }
    return 0;
}

static void dill_poller_rmin(int fd) {
    struct dill_crpair *crp = &dill_crpairs[fd];
    crp->in = NULL;
    if(!crp->next) {
        crp->next = dill_changelist;
        dill_changelist = fd + 1;
    }
}

static void dill_poller_rmout(int fd) {
    struct dill_crpair *crp = &dill_crpairs[fd];
    crp->out = NULL;
    if(!crp->next) {
        crp->next = dill_changelist;
        dill_changelist = fd + 1;
    }
}

static void dill_poller_clean(int fd) {
    struct dill_crpair *crp = &dill_crpairs[fd];
    dill_assert(!crp->in);
    dill_assert(!crp->out);
    /* Remove the file descriptor from the pollset, if it is still present. */
    if(crp->currevs) {   
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = 0;
        int rc = epoll_ctl(dill_efd, EPOLL_CTL_DEL, fd, &ev);
        dill_assert(rc == 0 || errno == ENOENT);
    }
    /* Clean the cache. */
    crp->currevs = 0;
    if(!crp->next) {
        crp->next = dill_changelist;
        dill_changelist = fd + 1;
    }
}

static int dill_poller_wait(int timeout) {
    /* Apply any changes to the pollset.
       TODO: Use epoll_ctl_batch once available. */
    while(dill_changelist != DILL_ENDLIST) {
        int fd = dill_changelist - 1;
        struct dill_crpair *crp = &dill_crpairs[fd];
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
            int rc = epoll_ctl(dill_efd, op, fd, &ev);
            dill_assert(rc == 0);
        }
        dill_changelist = crp->next;
        crp->next = 0;
    }
    /* Wait for events. */
    struct epoll_event evs[DILL_EPOLLSETSIZE];
    int numevs;
    while(1) {
        numevs = epoll_wait(dill_efd, evs, DILL_EPOLLSETSIZE, timeout);
        if(numevs < 0 && errno == EINTR)
            continue;
        dill_assert(numevs >= 0);
        break;
    }
    /* Fire file descriptor events. */
    int i;
    for(i = 0; i != numevs; ++i) {
        struct dill_crpair *crp = &dill_crpairs[evs[i].data.fd];
        /* Signal from parent. */
        if(dill_slow(crp == dill_parent)) {
            dill_cr_close_main();
            continue;
        }
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
        if(crp->in && inevents) {
            dill_resume(crp->in, inevents);
            dill_poller_rmin(evs[i].data.fd);
        }
        if(crp->out && outevents) {
            dill_resume(crp->out, outevents);
            dill_poller_rmout(evs[i].data.fd);
        }
    }
    /* Return 0 in case of time out. 1 if at least one coroutine was resumed. */
    return numevs > 0 ? 1 : 0;
}

