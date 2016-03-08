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
#include <sys/resource.h>
#include <sys/time.h>

#include "cr.h"
#include "utils.h"

#define DILL_ENDLIST 0xffffffff

#define DILL_CHNGSSIZE 128
#define DILL_EVSSIZE 128

static int dill_kfd = -1;

struct dill_crpair {
    struct dill_cr *in;
    struct dill_cr *out;
    uint16_t currevs;
    uint16_t firing;
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
    if(dill_fast(dill_kfd >= 0))
        return;
    struct rlimit rlim;
    int rc = getrlimit(RLIMIT_NOFILE, &rlim);
    if(dill_slow(rc < 0)) return;
    dill_ncrpairs = rlim.rlim_max;
    /* The above behaves weirdly on newer versions of OSX, ruturning limit
       of -1. Fix it by using OPEN_MAX instead. */
    if(dill_ncrpairs < 0)
        dill_ncrpairs = OPEN_MAX;
    dill_crpairs = (struct dill_crpair*)
        calloc(dill_ncrpairs, sizeof(struct dill_crpair));
    if(dill_slow(!dill_crpairs)) {errno = ENOMEM; return;}
    rc = atexit(dill_poller_atexit);
    dill_assert(rc == 0);
    dill_kfd = kqueue();
    if(dill_slow(dill_kfd < 0)) {
        free(dill_crpairs);
        dill_crpairs = NULL;
        return;
    }
    errno = 0;
}

void dill_poller_postfork(int parent) {
    if(dill_kfd != -1) {
        /* TODO: kqueue documentation says that a kqueue descriptor won't
           survive a fork. However, implementations seem to diverge.
           On FreeBSD the following function succeeds. On OSX it returns
           EACCESS. Therefore we ignore the return value. */
        close(dill_kfd);
    }
    dill_kfd = -1;
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

static int dill_poller_add(int fd, int events) {
    struct dill_crpair *crp = &dill_crpairs[fd];
    if(dill_slow((events & FDW_IN && crp->in) ||
          (events & FDW_OUT && crp->out))) {
        errno = EEXIST;
        return -1;
    }
    if(events & FDW_IN)
        crp->in = dill_running;
    if(events & FDW_OUT)
        crp->out = dill_running;
    if(!crp->next) {
        crp->next = dill_changelist;
        dill_changelist = fd + 1;
    }
    return 0;
}

static void dill_poller_rm(int fd, int events) {
    struct dill_crpair *crp = &dill_crpairs[fd];
    if(events & FDW_IN)
        crp->in = NULL;
    if(events & FDW_OUT)
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
    /* Remove the file descriptor from the pollset, if it is still there. */
    int nevs = 0;
    struct kevent evs[2];
    if(crp->currevs & FDW_IN) {
        EV_SET(&evs[nevs], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
        ++nevs;
    }
    if(crp->currevs & FDW_OUT) {
        EV_SET(&evs[nevs], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
        ++nevs;
    }
    if(nevs) {
        int rc = kevent(dill_kfd, evs, nevs, NULL, 0, NULL);
        dill_assert(rc != -1);
    }
    /* Clean up the cache. */
    crp->currevs = 0;
    if(!crp->next) {
        crp->next = dill_changelist;
        dill_changelist = fd + 1;
    }
}

static int dill_poller_wait(int timeout) {
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
        struct dill_crpair *crp = &dill_crpairs[fd];
        if(crp->in) {
            if(!(crp->currevs & FDW_IN)) {
                EV_SET(&chngs[nchngs], fd, EVFILT_READ, EV_ADD, 0, 0, 0);
                crp->currevs |= FDW_IN;
                ++nchngs;
            }
        }
        else {
            if(crp->currevs & FDW_IN) {
                EV_SET(&chngs[nchngs], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
                crp->currevs &= ~FDW_IN;
                ++nchngs;
            }
        }
        if(crp->out) {
            if(!(crp->currevs & FDW_OUT)) {
                EV_SET(&chngs[nchngs], fd, EVFILT_WRITE, EV_ADD, 0, 0, 0);
                crp->currevs |= FDW_OUT;
                ++nchngs;
           }
        }
        else {
            if(crp->currevs & FDW_OUT) {
                EV_SET(&chngs[nchngs], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
                crp->currevs &= ~FDW_OUT;
                ++nchngs;
            }
        }
        crp->firing = 0;
        dill_changelist = crp->next;
        crp->next = 0;
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
    }
    /* Join events on file descriptor basis. */
    int i;
    for(i = 0; i != nevs; ++i) {
        dill_assert(evs[i].flags != EV_ERROR);
        int fd = (int)evs[i].ident;
        struct dill_crpair *crp = &dill_crpairs[fd];
        /* Add firing event to the result list. */
        if(evs[i].flags == EV_EOF)
            crp->firing |= FDW_ERR;
        else {
            if(evs[i].filter == EVFILT_READ)
                crp->firing |= FDW_IN;
            if(evs[i].filter == EVFILT_WRITE)
                crp->firing |= FDW_OUT;
        }
        if(!crp->next) {
            crp->next = dill_changelist;
            dill_changelist = fd + 1;
        }
    }
    /* Resume the blocked coroutines. */
    uint32_t chl = dill_changelist;
    while(chl != DILL_ENDLIST) {
        int fd = chl - 1;
        struct dill_crpair *crp = &dill_crpairs[fd];
        /* Signal from parent. */
        if(dill_slow(crp == dill_parent)) {
            dill_cr_close_main();
        }
        else {
            if(crp->in == crp->out) {
                dill_assert(crp->in);
                dill_resume(crp->in, crp->firing);
                crp->in = NULL;
                crp->out = NULL;
            }
            else {
                if(crp->in) {
                    dill_resume(crp->in, crp->firing & (FDW_IN | FDW_ERR));
                    crp->in = NULL;
                }
                if(crp->out) {
                    dill_resume(crp->out, crp->firing & (FDW_OUT | FDW_ERR));
                    crp->out = NULL;
                }
            }
        }
        crp->firing = 0;
        chl = crp->next;
    }    
    /* Return 0 in case of time out. 1 if at least one coroutine was resumed. */
    return nevs > 0 ? 1 : 0;
}

