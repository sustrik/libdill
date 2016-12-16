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
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>

#include "cr.h"
#include "fd.h"
#include "list.h"
#include "pollset.h"
#include "utils.h"
#include "ctx.h"

#define DILL_ENDLIST 0xffffffff

#define DILL_CHNGSSIZE 128
#define DILL_EVSSIZE 128

#define FDW_IN 1
#define FDW_OUT 2

struct dill_fdinfo {
    struct dill_list in;
    struct dill_list out;
    uint16_t currevs;
    uint16_t firing;
    /* 1-based index, 0 stands for "not part of the list", DILL_ENDLIST
       stads for "no more elements in the list. */
    uint32_t next;
    /* 1 if the file descriptor is cached. 0 otherwise. */
    unsigned int cached : 1;
};

int dill_ctx_pollset_init(struct dill_ctx_pollset *ctx) {
    int err;
    /* Allocate one info per fd. */
    ctx->fdinfos = calloc(dill_maxfds(), sizeof(struct dill_fdinfo));
    if(dill_slow(!ctx->fdinfos)) {err = ENOMEM; goto error1;}
    /* Changelist is empty. */
    ctx->changelist = DILL_ENDLIST;
    /* Create kernel-side pollset. */
    ctx->kfd = kqueue();
    if(dill_slow(ctx->kfd < 0)) {err = errno; goto error2;}
    return 0;
error2:
    free(ctx->fdinfos);
    ctx->fdinfos = NULL;
error1:
    errno = err;
    return -1;
}

void dill_ctx_pollset_term(struct dill_ctx_pollset *ctx) {
    /* TODO: kqueue documentation says that a kqueue descriptor won't
       survive a fork. However, implementations seem to disagree.
       On FreeBSD the following function succeeds. On OSX it returns
       EACCESS. Therefore we ignore the return value. */
    close(ctx->kfd);
    free(ctx->fdinfos);
}

int dill_pollset_in(struct dill_clause *cl, int id, int fd) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
    if(!dill_list_empty(&fdi->in)) {errno = EEXIST; return -1;}
    /* If not yet cached check whether fd exists and if so add it to pollset. */
    if(dill_slow(!fdi->cached)) {
        struct kevent ev;
        EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, 0);
        int rc = kevent(ctx->kfd, &ev, 1, NULL, 0, NULL);
        if(dill_slow(rc < 0 && errno == EBADF)) return -1;
        dill_assert(rc >= 0);
        fdi->currevs = FDW_IN;
        fdi->firing = 0;
        fdi->next = 0;
        fdi->cached = 1;
    }
    /* If fd is not yet in the pollset add it there. */
    else if(!fdi->next) {
        fdi->next = ctx->changelist;
        ctx->changelist = fd + 1;
    }
    /* Add the clause to the list of waited for clauses. */
    dill_waitfor(cl, id, &fdi->in, NULL);
    return 0;
}

int dill_pollset_out(struct dill_clause *cl, int id, int fd) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
    if(!dill_list_empty(&fdi->out)) {errno = EEXIST; return -1;}
    /* If not yet cached check whether fd exists and if so add it to pollset. */    
    if(dill_slow(!fdi->cached)) {
        struct kevent ev;
        EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD, 0, 0, 0);
        int rc = kevent(ctx->kfd, &ev, 1, NULL, 0, NULL);
        if(dill_slow(rc < 0 && errno == EBADF)) return -1;
        dill_assert(rc >= 0);
        fdi->currevs = FDW_OUT;
        fdi->firing = 0;
        fdi->next = 0;
        fdi->cached = 1;
    }
    /* If fd is not yet in the pollset add it there. */
    else if(!fdi->next) {
        fdi->next = ctx->changelist;
        ctx->changelist = fd + 1;
    }
    /* Add the clause to the list of waited for clauses. */
    dill_waitfor(cl, id, &fdi->out, NULL);
    return 0;
}

void dill_pollset_clean(int fd) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
    /* We cannot clean an fd that someone is waiting for. */
    dill_assert(dill_list_empty(&fdi->in));
    dill_assert(dill_list_empty(&fdi->out));
    /* Remove the file descriptor from the pollset, if it is still there. */
    int nevs = 0;
    struct kevent evs[2];
    if(fdi->currevs & FDW_IN) {
        EV_SET(&evs[nevs], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
        ++nevs;
    }
    if(fdi->currevs & FDW_OUT) {
        EV_SET(&evs[nevs], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
        ++nevs;
    }
    if(nevs) {
        int rc = kevent(ctx->kfd, evs, nevs, NULL, 0, NULL);
        dill_assert(rc != -1);
    }
    fdi->currevs = 0;
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
}

int dill_pollset_poll(int timeout) {
    struct dill_ctx_pollset *ctx = &dill_getctx->pollset;
    /* Apply any changes to the pollset. */
    struct kevent chngs[DILL_CHNGSSIZE];
    int nchngs = 0;
    while(ctx->changelist != DILL_ENDLIST) {
        /* Flush the changes to the pollset even if there is one emtpy entry
           left in the changeset. That way we make sure that both in & out
           associated with the next file descriptor can be filled in if we
           choose not to flush the changes yet. */
        if(nchngs >= DILL_CHNGSSIZE - 1) {
            int rc = kevent(ctx->kfd, chngs, nchngs, NULL, 0, NULL);
            dill_assert(rc != -1);
            nchngs = 0;
        }
        int fd = ctx->changelist - 1;
        struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
        if(!dill_list_empty(&fdi->in)) {
            if(!(fdi->currevs & FDW_IN)) {
                EV_SET(&chngs[nchngs], fd, EVFILT_READ, EV_ADD, 0, 0, 0);
                fdi->currevs |= FDW_IN;
                ++nchngs;
            }
        }
        else {
            if(fdi->currevs & FDW_IN) {
                EV_SET(&chngs[nchngs], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
                fdi->currevs &= ~FDW_IN;
                ++nchngs;
            }
        }
        if(!dill_list_empty(&fdi->out)) {
            if(!(fdi->currevs & FDW_OUT)) {
                EV_SET(&chngs[nchngs], fd, EVFILT_WRITE, EV_ADD, 0, 0, 0);
                fdi->currevs |= FDW_OUT;
                ++nchngs;
           }
        }
        else {
            if(fdi->currevs & FDW_OUT) {
                EV_SET(&chngs[nchngs], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
                fdi->currevs &= ~FDW_OUT;
                ++nchngs;
            }
        }
        fdi->firing = 0;
        ctx->changelist = fdi->next;
        fdi->next = 0;
    }
    /* Wait for events. */
    struct kevent evs[DILL_EVSSIZE];
    struct timespec ts;
    if(timeout >= 0) {
        ts.tv_sec = timeout / 1000;
        ts.tv_nsec = (((long)timeout) % 1000) * 1000000;
    }
    int nevs = kevent(ctx->kfd, chngs, nchngs, evs, DILL_EVSSIZE,
        timeout < 0 ? NULL : &ts);
    if(nevs < 0 && errno == EINTR) return -1;
    dill_assert(nevs >= 0);
    /* Join events on file descriptor basis.
       Put all the firing fds into the changelist. */
    int i;
    for(i = 0; i != nevs; ++i) {
        dill_assert(evs[i].flags != EV_ERROR);
        int fd = (int)evs[i].ident;
        struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
        /* Add firing event to the result list. */
        if(evs[i].flags == EV_EOF)
            fdi->firing |= (FDW_IN | FDW_OUT);
        else {
            if(evs[i].filter == EVFILT_READ)
                fdi->firing |= FDW_IN;
            if(evs[i].filter == EVFILT_WRITE)
                fdi->firing |= FDW_OUT;
        }
        if(!fdi->next) {
            fdi->next = ctx->changelist;
            ctx->changelist = fd + 1;
        }
    }
    /* Resume the blocked coroutines. */
    uint32_t chl = ctx->changelist;
    while(chl != DILL_ENDLIST) {
        int fd = chl - 1;
        struct dill_fdinfo *fdi = &ctx->fdinfos[fd];
        if(!dill_list_empty(&fdi->in) && (fdi->firing & FDW_IN)) {
            struct dill_clause *cl = dill_cont(dill_list_begin(&fdi->in),
                struct dill_clause, epitem);
            dill_trigger(cl, 0);
        }
        if(!dill_list_empty(&fdi->out) && (fdi->firing & FDW_OUT)) {
            struct dill_clause *cl = dill_cont(dill_list_begin(&fdi->out),
                struct dill_clause, epitem);
            dill_trigger(cl, 0);
        }
        fdi->firing = 0;
        chl = fdi->next;
    }    
    /* Return 0 in case of time out. 1 if at least one coroutine was resumed. */
    return nevs > 0 ? 1 : 0;
}

