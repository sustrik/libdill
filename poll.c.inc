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
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "cr.h"
#include "list.h"
#include "pollset.h"
#include "utils.h"
#include "ctx.h"

/*

                                ctx->pollset_size
                                        |
  ctx->pollset                          V
  +-------+-------+-------+-----+-------+--------------------------------+
  | pfd 0 | pfd 1 | pfd 2 | ... | pfd N |             empty              |
  +-------+-------+-------+-----+-------+--------------------------------+
      ^                             ^                                    ^
      |                             |                                    |
     idx            +------idx------+                                    |
      |             |                                                    |
  +------+------+------+----------------------------------------+--------+
  | fd=0 | fd=1 | fd=2 |                   ...                  | fd=max |
  +------+------+------+----------------------------------------+--------+
  ctx->fdinfos                                                           ^
                                                                         |
                                                                  ctx->nfdinfos

*/

/* Additional info about file descriptor. */
struct dill_fdinfo {
    /* Index of the file descriptor in the pollset.
       -1 means the fd is not in the pollset. */
    int idx;
    /* Clause waiting for in. NULL if none. */
    struct dill_fdclause *in;
    /* Clause waiting for out. NULL if none. */
    struct dill_fdclause *out;
    /* 1 is the file descriptor was used before, 0 otherwise. */
    unsigned int cached : 1;
};

int dill_ctx_pollset_init(struct dill_ctx_pollset *ctx) {
    int err;
    ctx->nfdinfos = dill_maxfds();
    /* Allocate largest possible pollset. */
    ctx->pollset_size = 0;
    ctx->pollset = malloc(sizeof(struct pollfd) * ctx->nfdinfos);
    if(dill_slow(!ctx->pollset)) {err = ENOMEM; goto error1;}
    ctx->fdinfos = malloc(sizeof(struct dill_fdinfo) * ctx->nfdinfos);
    if(dill_slow(!ctx->fdinfos)) {err = ENOMEM; goto error2;}
    /* Intialise fd infos. There's no fd in the pollset,
       so set all indices to -1. */
    int i;
    for(i = 0; i != ctx->nfdinfos; ++i) {
        ctx->fdinfos[i].idx = -1;
        ctx->fdinfos[i].in = NULL;
        ctx->fdinfos[i].out = NULL;
        ctx->fdinfos[i].cached = 0;
    }
    return 0;
error2:
    free(ctx->pollset);
    ctx->pollset = NULL;
error1:
    errno = err;
    return -1;
}

void dill_ctx_pollset_term(struct dill_ctx_pollset *ctx) {
    free(ctx->pollset);
    free(ctx->fdinfos);
}

static void dill_fdcancelin(struct dill_clause *cl) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    struct dill_fdinfo *fdi = dill_cont(cl, struct dill_fdclause, cl)->fdinfo;
    fdi->in = NULL;
    ctx->pollset[fdi->idx].events &= ~POLLIN;
    /* fd is left in the pollset. It will be purged once the event loop
       iterates once more. */
}

static void dill_fdcancelout(struct dill_clause *cl) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    struct dill_fdinfo *fdi = dill_cont(cl, struct dill_fdclause, cl)->fdinfo;
    fdi->out = NULL;
    ctx->pollset[fdi->idx].events &= ~POLLOUT;
    /* fd is left in the pollset. It will be purged once the event loop
       iterates once more. */
}

int dill_pollset_in(struct dill_fdclause *fdcl, int id, int fd) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    if(dill_slow(fd < 0 || fd >= ctx->nfdinfos)) {errno = EBADF; return -1;}
    struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
    if(dill_slow(!fdi->cached)) {
        int flags = fcntl(fd, F_GETFD);
        if(flags < 0 && errno == EBADF) return -1;
        dill_assert(flags >= 0);
        fdi->cached = 1;
    }
    if(fdi->idx < 0) {
        fdi->idx = ctx->pollset_size;
        ++ctx->pollset_size;
        ctx->pollset[fdi->idx].fd = fd;
    }
    if(dill_slow(fdi->in)) {errno = EBUSY; return -1;}
    ctx->pollset[fdi->idx].events |= POLLIN;
    fdcl->fdinfo = fdi;
    fdi->in = fdcl;
    dill_waitfor(&fdcl->cl, id, dill_fdcancelin);
    return 0;
}

int dill_pollset_out(struct dill_fdclause *fdcl, int id, int fd) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    if(dill_slow(fd < 0 || fd >= ctx->nfdinfos)) {errno = EBADF; return -1;}
    struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
    if(dill_slow(!fdi->cached)) {
        int flags = fcntl(fd, F_GETFD);
        if(flags < 0 && errno == EBADF) return -1;
        dill_assert(flags >= 0);
        fdi->cached = 1;
    }
    if(fdi->idx < 0) {
        fdi->idx = ctx->pollset_size;
        ++ctx->pollset_size;
        ctx->pollset[fdi->idx].fd = fd;
    }
    if(dill_slow(fdi->out)) {errno = EBUSY; return -1;}
    ctx->pollset[fdi->idx].events |= POLLOUT;
    fdcl->fdinfo = fdi;
    fdi->out = fdcl;
    dill_waitfor(&fdcl->cl, id, dill_fdcancelout);
    return 0;
}

int dill_pollset_clean(int fd) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
    if(!fdi->cached) return 0;
    if(dill_slow(fdi->in || fdi->out)) {errno = EBUSY; return -1;}
    /* If the fd happens to still be in the pollset remove it. */
    if(fdi->idx >= 0) {
        --ctx->pollset_size;
        if(fdi->idx != ctx->pollset_size) {
            struct pollfd *pfd = &ctx->pollset[fdi->idx];
            struct pollfd *lastpfd = &ctx->pollset[ctx->pollset_size];
            *pfd = *lastpfd;
            ctx->fdinfos[pfd->fd].idx = fdi->idx;
        }
        fdi->idx = -1;
    }
    fdi->cached = 0;
    return 0;
}

int dill_pollset_poll(int timeout) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    /* Wait for events. */
    int numevs = poll(ctx->pollset, ctx->pollset_size, timeout);
    if(numevs < 0 && errno == EINTR) return -1;
    dill_assert(numevs >= 0);
    /* Fire file descriptor events as needed. */
    int i;
    for(i = 0; i != ctx->pollset_size; ++i) {
        struct pollfd *pfd = &ctx->pollset[i];
        struct dill_fdinfo *fdi = &ctx->fdinfos[pfd->fd];
        /* Resume the blocked coroutines. */
        if(fdi->in &&
              pfd->revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) {
            pfd->events &= ~POLLIN;
            dill_trigger(&fdi->in->cl, 0);
        }
        if(fdi->out && 
              pfd->revents & (POLLOUT | POLLERR | POLLHUP | POLLNVAL)) {
            pfd->events &= ~POLLOUT;
            dill_trigger(&fdi->out->cl, 0);
        }
        /* If nobody is polling for the fd remove it from the pollset. */
        if(!pfd->events) {
            fdi->idx = -1;
            dill_assert(!fdi->in && !fdi->out);
            --ctx->pollset_size;
            /* Pollset has to be compact. Thus, unless we are removing the
               last item from the pollset we want to move the last item
               to the vacant slot left by the removed fd. */
            if(i != ctx->pollset_size) {
                struct pollfd *lastpfd = &ctx->pollset[ctx->pollset_size];
                *pfd = *lastpfd;
                ctx->fdinfos[pfd->fd].idx = i;
            }
            --i;
        }
    }
    return numevs > 0;
}

