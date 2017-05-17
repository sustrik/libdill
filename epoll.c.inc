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
#include <unistd.h>

#include "cr.h"
#include "list.h"
#include "pollset.h"
#include "utils.h"
#include "ctx.h"

#define DILL_ENDLIST 0xffffffff

#define DILL_EPOLLSETSIZE 128

/* One of these is associated with each file descriptor. */
struct dill_fdinfo {
    /* A coroutines waiting to read from the fd or NULL. */
    struct dill_fdclause *in;
    /* A coroutines waiting to write to the fd or NULL. */
    struct dill_fdclause *out;
    /* Cached current state of epollset. */
    uint32_t currevs;
    /* 1-based index, 0 stands for "not part of the list", DILL_ENDLIST
       stands for "no more elements in the list. */
    uint32_t next;
    /* 1 if the file descriptor is cached. 0 otherwise. */
    unsigned int cached : 1;
};

int dill_ctx_pollset_init(struct dill_ctx_pollset *ctx) {
    int err;
    /* Allocate one info per fd. */
    ctx->nfdinfos = dill_maxfds();
    ctx->fdinfos = calloc(ctx->nfdinfos, sizeof(struct dill_fdinfo));
    if(dill_slow(!ctx->fdinfos)) {err = ENOMEM; goto error1;}
    /* Changelist is empty. */
    ctx->changelist = DILL_ENDLIST;
    /* Create the kernel-side pollset. */
    ctx->efd = epoll_create(1);
    if(dill_slow(ctx->efd < 0)) {err = errno; goto error2;}
    return 0;
error2:
    free(ctx->fdinfos);
    ctx->fdinfos = NULL;
error1:
    errno = err;
    return -1;
}

void dill_ctx_pollset_term(struct dill_ctx_pollset *ctx) {
    int rc = close(ctx->efd);
    dill_assert(rc == 0);
    free(ctx->fdinfos);
}

static void dill_fdcancelin(struct dill_clause *cl) {
    struct dill_fdinfo *fdinfo =
        dill_cont(cl, struct dill_fdclause, cl)->fdinfo;
    fdinfo->in = NULL;
    if(!fdinfo->next) {
        struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
        fdinfo->next = ctx->changelist;
        ctx->changelist = fdinfo - ctx->fdinfos + 1;
    }
}

static void dill_fdcancelout(struct dill_clause *cl) {
    struct dill_fdinfo *fdinfo =
        dill_cont(cl, struct dill_fdclause, cl)->fdinfo;
    fdinfo->out = NULL;
    if(!fdinfo->next) {
        struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
        fdinfo->next = ctx->changelist;
        ctx->changelist = fdinfo - ctx->fdinfos + 1;
    }
}

int dill_pollset_in(struct dill_fdclause *fdcl, int id, int fd) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    if(dill_slow(fd < 0 || fd >= ctx->nfdinfos)) {errno = EBADF; return -1;}
    struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
    /* If not yet cached, check whether the fd exists and if it does,
       add it to the pollset. */
    if(dill_slow(!fdi->cached)) {
        struct epoll_event ev;
#ifdef DILL_VALGRIND
        memset(&ev.data, 0, sizeof(ev.data)); //Keep Valgrind happy
#endif
        ev.data.fd = fd;
        ev.events = EPOLLIN;
        int rc = epoll_ctl(ctx->efd, EPOLL_CTL_ADD, fd, &ev);
        if(dill_slow(rc < 0)) {
            if(errno == ELOOP || errno == EPERM) {errno = ENOTSUP; return -1;}
            return -1;
        }
        fdi->in = NULL;
        fdi->out = NULL;
        fdi->currevs = EPOLLIN;
        fdi->next = 0;
        fdi->cached = 1;
    }
    if(dill_slow(fdi->in)) {errno = EBUSY; return -1;}
    /* If the fd is not yet in the pollset, add it there. */
    else if(!fdi->next) {
        fdi->next = ctx->changelist;
        ctx->changelist = fd + 1;
    }
    fdcl->fdinfo = fdi;
    fdi->in = fdcl;
    dill_waitfor(&fdcl->cl, id, dill_fdcancelin);
    return 0;
}

int dill_pollset_out(struct dill_fdclause *fdcl, int id, int fd) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    if(dill_slow(fd < 0 || fd >= ctx->nfdinfos)) {errno = EBADF; return -1;}
    struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
    /* If not yet cached, check whether the fd exists and if it does,
       add it to pollset. */
    if(dill_slow(!fdi->cached)) {
        struct epoll_event ev;
#ifdef DILL_VALGRIND
        memset(&ev.data, 0, sizeof(ev.data)); //Keep Valgrind happy
#endif
        ev.data.fd = fd;
        ev.events = EPOLLOUT;
        int rc = epoll_ctl(ctx->efd, EPOLL_CTL_ADD, fd, &ev);
        if(dill_slow(rc < 0)) {
            if(errno == ELOOP || errno == EPERM) {errno = ENOTSUP; return -1;}
            return -1;
        }
        fdi->in = NULL;
        fdi->out = NULL;
        fdi->currevs = EPOLLOUT;
        fdi->next = 0;
        fdi->cached = 1;
    }
    if(dill_slow(fdi->out)) {errno = EBUSY; return -1;}
    /* If the fd is not yet in the pollset, add it there. */
    else if(!fdi->next) {
        fdi->next = ctx->changelist;
        ctx->changelist = fd + 1;
    }
    fdcl->fdinfo = fdi;
    fdi->out = fdcl;
    dill_waitfor(&fdcl->cl, id, dill_fdcancelout);
    return 0;
}

int dill_pollset_clean(int fd) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
    if(!fdi->cached) return 0;
    /* We cannot clean an fd that someone is waiting for. */
    if(dill_slow(fdi->in || fdi->out)) {errno = EBUSY; return -1;}
    /* Remove the file descriptor from the pollset if it is still there. */
    if(fdi->currevs) {
        struct epoll_event ev;
#ifdef DILL_VALGRIND
        memset(&ev.data, 0, sizeof(ev.data)); //Keep Valgrind happy
#endif
        ev.data.fd = fd;
        ev.events = 0;
        int rc = epoll_ctl(ctx->efd, EPOLL_CTL_DEL, fd, &ev);
        dill_assert(rc == 0 || errno == ENOENT);
        fdi->currevs = 0;
    }
    /* If needed, remove the fd from the changelist. */
    if(fdi->next) {
        uint32_t *pidx = &ctx->changelist;
        while(1) {
            dill_assert(*pidx != 0 && *pidx != DILL_ENDLIST);
            if(*pidx - 1 == fd) break;
            pidx = &ctx->fdinfos[*pidx - 1].next;
        }
        *pidx = fdi->next;
        fdi->next = 0;
    }
    /* Mark the fd as not used. */
    fdi->cached = 0;
    return 0;
}

int dill_pollset_poll(int timeout) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    /* Apply any changes to the pollset.
       TODO: Use epoll_ctl_batch once available. */
    while(ctx->changelist != DILL_ENDLIST) {
        int fd = ctx->changelist - 1;
        struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
        struct epoll_event ev;
        ev.data.u64 = 0; //Keep Valgrind happy
        ev.data.fd = fd;
        ev.events = 0;
        if(fdi->in)
            ev.events |= EPOLLIN;
        if(fdi->out)
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
            int rc = epoll_ctl(ctx->efd, op, fd, &ev);
            dill_assert(rc == 0);
        }
        ctx->changelist = fdi->next;
        fdi->next = 0;
    }
    /* Wait for events. */
    struct epoll_event evs[DILL_EPOLLSETSIZE];
    int numevs = epoll_wait(ctx->efd, evs, DILL_EPOLLSETSIZE, timeout);
    if(numevs < 0 && errno == EINTR) return -1;
    dill_assert(numevs >= 0);
    /* Fire file descriptor events. */
    int i;
    for(i = 0; i != numevs; ++i) {
        int fd = evs[i].data.fd;
        struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
        /* Resume blocked coroutines. */
        if(fdi->in && (evs[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP))) {
            dill_trigger(&fdi->in->cl, 0);
            /* Remove the fd from the pollset if needed. */
            if(!fdi->in && !fdi->next) {
                fdi->next = ctx->changelist;
                ctx->changelist = fd + 1;
            }
        }
        if(fdi->out && (evs[i].events & (EPOLLOUT | EPOLLERR | EPOLLHUP))) {
            dill_trigger(&fdi->out->cl, 0);
            /* Remove the fd from the pollset if needed. */
            if(!fdi->out && !fdi->next) {
                fdi->next = ctx->changelist;
                ctx->changelist = fd + 1;
            }
        }
    }
    /* Return 0 on timeout or 1 if at least one coroutine was resumed. */
    return numevs > 0 ? 1 : 0;
}
