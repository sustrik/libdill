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
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "cr.h"
#include "utils.h"

#define TS_ENDLIST 0xffffffff

#define TS_CHNGSSIZE 128
#define TS_EVSSIZE 128

static int ts_kfd = -1;

struct ts_crpair {
    struct ts_cr *in;
    struct ts_cr *out;
    uint16_t currevs;
    uint16_t firing;
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
    /* The above behaves weirdly on newer versions of OSX, ruturning limit
       of -1. Fix it by using OPEN_MAX instead. */
    if(ts_ncrpairs < 0)
        ts_ncrpairs = OPEN_MAX;
    ts_crpairs = (struct ts_crpair*)
        calloc(ts_ncrpairs, sizeof(struct ts_crpair));
    if(ts_slow(!ts_crpairs)) {errno = ENOMEM; return;}
    ts_kfd = kqueue();
    if(ts_slow(ts_kfd < 0)) {
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
    if(ts_kfd != -1) {
        /* TODO: kqueue documentation says that a kqueue descriptor won't
           survive a fork. If that was so, one would expect following close()
           to fail with EBADF error. Yet, at least on FreeBSD, the call
           succeeds. */
        int rc = close(ts_kfd);
        ts_assert(rc == 0);
        ts_kfd = kqueue();
        ts_assert(ts_kfd >= 0);
        struct kevent chngs[TS_CHNGSSIZE];
        int nchngs = 0;
        int fd;
        for(fd = 0; fd != ts_ncrpairs; ++fd) {
            struct ts_crpair *crp = &ts_crpairs[fd];
            if(nchngs >= TS_CHNGSSIZE - 1) {
                rc = kevent(ts_kfd, chngs, nchngs, NULL, 0, NULL);
                ts_assert(rc != -1);
                nchngs = 0;
            }
            if(crp->currevs & FDW_IN) {
                EV_SET(&chngs[nchngs], fd, EVFILT_READ, EV_ADD, 0, 0, 0);
                ++nchngs;
            }
            if(crp->currevs & FDW_OUT) {
                EV_SET(&chngs[nchngs], fd, EVFILT_WRITE, EV_ADD, 0, 0, 0);
                ++nchngs;
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
        int rc = kevent(ts_kfd, evs, nevs, NULL, 0, NULL);
        ts_assert(rc != -1);
    }
    /* Clean up the cache. */
    crp->currevs = 0;
    if(!crp->next) {
        crp->next = ts_changelist;
        ts_changelist = fd + 1;
    }
}

static int ts_poller_wait(int timeout) {
    /* Apply any changes to the pollset. */
    struct kevent chngs[TS_CHNGSSIZE];
    int nchngs = 0;
    while(ts_changelist != TS_ENDLIST) {
        /* Flush the changes to the pollset even if there is one emtpy entry
           left in the changeset. That way we make sure that both in & out
           associated with the next file descriptor can be filled in if we
           choose not to flush the changes yet. */
        if(nchngs >= TS_CHNGSSIZE - 1) {
            int rc = kevent(ts_kfd, chngs, nchngs, NULL, 0, NULL);
            ts_assert(rc != -1);
            nchngs = 0;
        }
        int fd = ts_changelist - 1;
        struct ts_crpair *crp = &ts_crpairs[fd];
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
        ts_changelist = crp->next;
        crp->next = 0;
    }
    /* Wait for events. */
    struct kevent evs[TS_EVSSIZE];
    int nevs;
    while(1) {
        struct timespec ts;
        if(timeout >= 0) {
            ts.tv_sec = timeout / 1000;
            ts.tv_nsec = (((long)timeout) % 1000) * 1000000;
        }
        nevs = kevent(ts_kfd, chngs, nchngs, evs, TS_EVSSIZE,
            timeout < 0 ? NULL : &ts);
        if(nevs < 0 && errno == EINTR)
            continue;
        ts_assert(nevs >= 0);
        break;
    }
    /* Join events on file descriptor basis. */
    int i;
    for(i = 0; i != nevs; ++i) {
        ts_assert(evs[i].flags != EV_ERROR);
        int fd = (int)evs[i].ident;
        struct ts_crpair *crp = &ts_crpairs[fd];
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
            crp->next = ts_changelist;
            ts_changelist = fd + 1;
        }
    }
    /* Resume the blocked coroutines. */
    uint32_t chl = ts_changelist;
    while(chl != TS_ENDLIST) {
        int fd = chl - 1;
        struct ts_crpair *crp = &ts_crpairs[fd];
        if(crp->in == crp->out) {
            ts_assert(crp->in);
            ts_resume(crp->in, crp->firing);
            crp->in = NULL;
            crp->out = NULL;
        }
        else {
            if(crp->in) {
                ts_resume(crp->in, crp->firing & (FDW_IN | FDW_ERR));
                crp->in = NULL;
            }
            if(crp->out) {
                ts_resume(crp->out, crp->firing & (FDW_OUT | FDW_ERR));
                crp->out = NULL;
            }
        }
        crp->firing = 0;
        chl = crp->next;
    }    
    /* Return 0 in case of time out. 1 if at least one coroutine was resumed. */
    return nevs > 0 ? 1 : 0;
}

