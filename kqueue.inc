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

#include "cr.h"
#include "list.h"
#include "utils.h"

#define DILL_ENDLIST 0xffffffff

#define DILL_CHNGSSIZE 128
#define DILL_EVSSIZE 128

#define FDW_IN 1
#define FDW_OUT 2

static int dill_kfd = -1;

struct dill_fdinfo {
    struct dill_list in;
    struct dill_list out;
    uint16_t currevs;
    uint16_t firing;
    /* 1-based index, 0 stands for "not part of the list", DILL_ENDLIST
       stads for "no more elements in the list. */
    uint32_t next;
};

static struct dill_fdinfo *dill_fdinfos = NULL;
static uint32_t dill_changelist = DILL_ENDLIST;
static struct dill_fdinfo *dill_parentinfo = NULL;

int dill_pollset_init(void) {
    int err;
    dill_fdinfos = (struct dill_fdinfo*)
        calloc(dill_maxfds, sizeof(struct dill_fdinfo));
    if(dill_slow(!dill_fdinfos)) {err = ENOMEM; goto error1;}
    dill_kfd = kqueue();
    if(dill_slow(dill_kfd < 0)) {err = errno; goto error2;}
    dill_changelist = DILL_ENDLIST;
    /* Start polling for signals from the parent. */
    dill_parentinfo = &dill_fdinfos[dill_parent];
    dill_parentinfo->next = dill_changelist;
    dill_changelist = dill_parent + 1;
    return 0;
error2:
    free(dill_fdinfos);
    dill_fdinfos = NULL;
error1:
    errno = err;
    return -1;
}

void dill_pollset_term(void) {
    free(dill_fdinfos);
    dill_fdinfos = NULL;
    if(dill_kfd != -1) {
        /* TODO: kqueue documentation says that a kqueue descriptor won't
           survive a fork. However, implementations seem to diverge.
           On FreeBSD the following function succeeds. On OSX it returns
           EACCESS. Therefore we ignore the return value. */
        close(dill_kfd);
    }
    dill_kfd = -1;
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

#if 0
static void dill_poller_rmin(int fd) {
    struct dill_fdinfo *crp = &dill_fdinfos[fd];
    crp->in = NULL;
    if(!crp->next) {
        crp->next = dill_changelist;
        dill_changelist = fd + 1;
    }
}
#endif

static void dill_pollset_clean(int fd) {
    struct dill_fdinfo *fdi = &dill_fdinfos[fd];
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
        int rc = kevent(dill_kfd, evs, nevs, NULL, 0, NULL);
        dill_assert(rc != -1);
    }
    /* Clean up the cache. */
    fdi->currevs = 0;
    if(!fdi->next) {
        fdi->next = dill_changelist;
        dill_changelist = fd + 1;
    }
}

static int dill_pollset_poll(int timeout) {
    /* Apply any changes to the pollset. */
    struct kevent chngs[DILL_CHNGSSIZE];
    int nchngs = 0;
    while(dill_changelist != DILL_ENDLIST) {
        /* Flush the changes to the pollset even if there is one emtpy entry
           left in the changeset. That way we make sure that both in & out
           associated with the next file descriptor can be filled in if we
           choose not to flush the changes yet. */
        if(nchngs >= DILL_CHNGSSIZE - 1) {
            int rc = kevent(dill_kfd, chngs, nchngs, NULL, 0, NULL);
            dill_assert(rc != -1);
            nchngs = 0;
        }
        int fd = dill_changelist - 1;
        struct dill_fdinfo *fdi = &dill_fdinfos[fd];
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
        dill_changelist = fdi->next;
        fdi->next = 0;
    }
    /* Wait for events. */
    struct kevent evs[DILL_EVSSIZE];
    int nevs;
    while(1) {
        struct timespec ts;
        if(timeout >= 0) {
            ts.tv_sec = timeout / 1000;
            ts.tv_nsec = (((long)timeout) % 1000) * 1000000;
        }
        nevs = kevent(dill_kfd, chngs, nchngs, evs, DILL_EVSSIZE,
            timeout < 0 ? NULL : &ts);
        if(nevs < 0 && errno == EINTR)
            continue;
        dill_assert(nevs >= 0);
        break;
        /* TODO: Decrease timeout! */
    }
    /* Join events on file descriptor basis.
       Put all the firing fds into the changelist. */
    int i;
    for(i = 0; i != nevs; ++i) {
        dill_assert(evs[i].flags != EV_ERROR);
        int fd = (int)evs[i].ident;
        struct dill_fdinfo *fdi = &dill_fdinfos[fd];
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
            fdi->next = dill_changelist;
            dill_changelist = fd + 1;
        }
    }
    /* Resume the blocked coroutines. */
    uint32_t chl = dill_changelist;
    while(chl != DILL_ENDLIST) {
        int fd = chl - 1;
        struct dill_fdinfo *fdi = &dill_fdinfos[fd];
        /* Signal from parent. */
        if(dill_slow(fdi == dill_parentinfo)) {
            dill_parentinfo = NULL;
            dill_shutdown();
        }
        else {            
            if(!dill_list_empty(&fdi->in) && (fdi->firing & FDW_IN)) {
                struct dill_clause *cl = dill_cont(dill_list_begin(&fdi->in),
                    struct dill_clause, epitem);
                dill_trigger(cl, 0);
            }
            if(!dill_list_empty(&fdi->out) && (fdi->firing & FDW_OUT)) {
                struct dill_clause *cl = dill_cont(dill_list_begin(&fdi->in),
                    struct dill_clause, epitem);
                dill_trigger(cl, 0);
            }
        }
        fdi->firing = 0;
        chl = fdi->next;
    }    
    /* Return 0 in case of time out. 1 if at least one coroutine was resumed. */
    return nevs > 0 ? 1 : 0;
}

