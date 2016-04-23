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

#include "cr.h"
#include "list.h"
#include "utils.h"

#define DILL_ENDLIST 0xffffffff

#define DILL_EPOLLSETSIZE 128

/* Global pollset. */
static int dill_efd = -1;

/* One of these is associated with each file descriptor. */
struct dill_fdinfo {
    /* List of coroutines waiting to read from fd. */
    struct dill_list in;
    /* List of coroutines waiting to write to fd. */
    struct dill_list out;
    /* Cached current state of epollset. */
    uint32_t currevs;
    /* 1-based index, 0 stands for "not part of the list", DILL_ENDLIST
       stands for "no more elements in the list. */
    uint32_t next;
};

static struct dill_fdinfo *dill_fdinfos = NULL;
static uint32_t dill_changelist = DILL_ENDLIST;
static struct dill_fdinfo *dill_parentinfo = NULL;

static int dill_pollset_init(void) {
    int err;
    /* Allocate one info per fd. */
    dill_fdinfos = (struct dill_fdinfo*)
        calloc(dill_maxfds, sizeof(struct dill_fdinfo));
    if(dill_slow(!dill_fdinfos)) {err = ENOMEM; goto error1;}
    /* Changelist is empty. */
    dill_changelist = DILL_ENDLIST;
    /* Create kernel-side pollset. */
    dill_efd = epoll_create(1);
    if(dill_slow(dill_efd < 0)) {err = errno; goto error2;}
    /* Start polling for signals from the parent. */
    if(dill_parent >= 0) {
        dill_parentinfo = &dill_fdinfos[dill_parent];
        dill_parentinfo->next = dill_changelist;
        dill_changelist = dill_parent + 1;
    }
    return 0;
error2:
    free(dill_fdinfos);
    dill_fdinfos = NULL;
error1:
    errno = err;
    return -1;
}

static void dill_pollset_term(void) {
    if(dill_efd >= 0) {
        int rc = close(dill_efd);
        dill_assert(rc == 0);
        dill_efd = -1;
    }
    free(dill_fdinfos);
    dill_fdinfos = NULL;
    dill_changelist = DILL_ENDLIST;
    dill_parentinfo = NULL;
}

static int dill_pollset_in(struct dill_clause *cl, int id, int fd) {
    struct dill_fdinfo *fdi = &dill_fdinfos[fd];
    if(!dill_list_empty(&fdi->in)) {errno = EEXIST; return -1;}
    /* If fd is not yet in the pollset add it there. */
    if(!fdi->next) {
        fdi->next = dill_changelist;
        dill_changelist = fd + 1;
    }
    /* Add the clause to the list of waited for clauses. */
    dill_waitfor(cl, id, &fdi->in, NULL);
    return 0;
}

static int dill_pollset_out(struct dill_clause *cl, int id, int fd) {
    struct dill_fdinfo *fdi = &dill_fdinfos[fd];
    if(!dill_list_empty(&fdi->out)) {errno = EEXIST; return -1;}
    /* If fd is not yet in the pollset add it there. */
    if(!fdi->next) {
        fdi->next = dill_changelist;
        dill_changelist = fd + 1;
    }
    /* Add the clause to the list of waited for clauses. */
    dill_waitfor(cl, id, &fdi->out, NULL);
    return 0;
}

static void dill_pollset_clean(int fd) {
    struct dill_fdinfo *fdi = &dill_fdinfos[fd];
    /* We cannot clean an fd that someone is waiting for. */
    dill_assert(dill_list_empty(&fdi->in));
    dill_assert(dill_list_empty(&fdi->out));
    /* Remove the file descriptor from the pollset, if it is still present. */
    if(fdi->currevs) {   
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = 0;
        int rc = epoll_ctl(dill_efd, EPOLL_CTL_DEL, fd, &ev);
        dill_assert(rc == 0 || errno == ENOENT);
    }
    /* Clean the cache. */
    fdi->currevs = 0;
    if(!fdi->next) {
        fdi->next = dill_changelist;
        dill_changelist = fd + 1;
    }
}

static int dill_pollset_poll(int timeout) {
    /* Apply any changes to the pollset.
       TODO: Use epoll_ctl_batch once available. */
    while(dill_changelist != DILL_ENDLIST) {
        int fd = dill_changelist - 1;
        struct dill_fdinfo *fdi = &dill_fdinfos[fd];
        struct epoll_event ev;
        ev.data.fd = fd;
        ev.events = 0;
        if(!dill_list_empty(&fdi->in) || fdi == dill_parentinfo)
            ev.events |= EPOLLIN;
        if(!dill_list_empty(&fdi->out))
            ev.events |= EPOLLOUT;
        if(fdi->currevs != ev.events) {
            int op;
            if(!ev.events)
                 op = EPOLL_CTL_DEL;
            else if(!fdi->currevs)
                 op = EPOLL_CTL_ADD;
            else
                 op = EPOLL_CTL_MOD;
            fdi->currevs = ev.events;
            int rc = epoll_ctl(dill_efd, op, fd, &ev);
            dill_assert(rc == 0);
        }
        dill_changelist = fdi->next;
        fdi->next = 0;
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
        int fd = evs[i].data.fd;
        struct dill_fdinfo *fdi = &dill_fdinfos[fd];
        /* Signal from parent process. */
        if(dill_slow(fdi == dill_parentinfo)) {
            dill_parentinfo = NULL;
            dill_shutdown();
            continue;
        }
        /* Resume the blocked coroutines. */
        if(!dill_list_empty(&fdi->in) &&
              (evs[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP))) {
            struct dill_clause *cl = dill_cont(dill_list_begin(&fdi->in),
                struct dill_clause, epitem);
            dill_trigger(cl, 0);
            /* Remove the fd from the pollset, if needed. */
            if(dill_list_empty(&fdi->in) && !fdi->next) {
                fdi->next = dill_changelist;
                dill_changelist = fd + 1;
            }
        }
        if(!dill_list_empty(&fdi->out) &&
              (evs[i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP))) {
            struct dill_clause *cl = dill_cont(dill_list_begin(&fdi->out),
                struct dill_clause, epitem);
            dill_trigger(cl, 0);
            /* Remove the fd from the pollset, if needed. */
            if(dill_list_empty(&fdi->out) && !fdi->next) {
                fdi->next = dill_changelist;
                dill_changelist = fd + 1;
            }
        }
    }
    /* Return 0 in case of time out. 1 if at least one coroutine was resumed. */
    return numevs > 0 ? 1 : 0;
}

