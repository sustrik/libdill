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
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>

#include "cr.h"
#include "fd.h"
#include "list.h"
#include "utils.h"

/*

                                dill_pollset_size
                                        |
  dill_pollset                          V
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
  dill_fdinfos                                                           ^
                                                                         |
                                                                  dill_maxfds

*/

static int dill_parent = -1;

/* Pollset, as used by poll(2). */
static int dill_pollset_size = 0;
static struct pollfd *dill_pollset = NULL;

/* Additional info about file descriptor. */
struct dill_fdinfo {
    /* Index of the file descriptor in the pollset.
       -1 means the fd is not in the pollset. */
    int idx;
    /* Clauses waiting for in. */
    struct dill_list in;
    /* Clauses waiting for out. */
    struct dill_list out;
    /* 1 is the file descriptor was used before, 0 otherwise. */
    unsigned int cached : 1;
};

/* Info about all file descriptors.
   File descriptors are used as indices in this array. */
static struct dill_fdinfo *dill_fdinfos = NULL;

#if defined DILL_VALGRIND

static void dill_pollset_atexit(void) {
    if(dill_pollset)
        free(dill_pollset);
    if(dill_fdinfos)
        free(dill_fdinfos);
}

#endif

int dill_pollset_init(int parent) {
    int err;
    /* Allocate largest possible pollset. */
    dill_pollset_size = 0;
    dill_pollset = malloc(sizeof(struct pollfd) * dill_maxfds());
    if(dill_slow(!dill_pollset)) {err = ENOMEM; goto error1;}
    dill_fdinfos = malloc(sizeof(struct dill_fdinfo) * dill_maxfds());
    if(dill_slow(!dill_fdinfos)) {err = ENOMEM; goto error2;}
#if defined DILL_VALGRIND
    static int initialized = 0;
    if(dill_slow(!initialized)) {
        int rc = atexit(dill_pollset_atexit);
        dill_assert(rc == 0);
        initialized = 1;
    }
#endif
    /* Intialise fd infos. There's no fd in the pollset,
       so set all indices to -1. */
    int i;
    for(i = 0; i != dill_maxfds(); ++i) {
        dill_fdinfos[i].idx = -1;
        dill_list_init(&dill_fdinfos[i].in);
        dill_list_init(&dill_fdinfos[i].out);
        dill_fdinfos[i].cached = 0;
    }
    /* Start waiting for a signal from the parent process. */
    if(parent >= 0) {
        dill_parent = parent;
        dill_pollset[0].fd = parent;
        dill_pollset[0].events = POLLIN;
        dill_pollset_size = 1;
    }
    return 0;
error2:
    free(dill_pollset);
    dill_pollset = NULL;
error1:
    errno = err;
    return -1;
}

void dill_pollset_term(void) {
    free(dill_pollset);
    dill_pollset = NULL;
    free(dill_fdinfos);
    dill_fdinfos = NULL;
}

int dill_pollset_in(struct dill_clause *cl, int id, int fd) {
    struct dill_fdinfo *fdi = &dill_fdinfos[fd];
    if(dill_slow(!fdi->cached)) {
        int flags = fcntl(fd, F_GETFD);
        if(flags < 0 && errno == EBADF) return -1;
        dill_assert(flags >= 0);
        fdi->cached = 1;
    }
    if(fdi->idx < 0) {
        dill_assert(dill_pollset_size < dill_maxfds());
        fdi->idx = dill_pollset_size;
        ++dill_pollset_size;
        dill_pollset[fdi->idx].fd = fd;
    }
    if(dill_slow(!dill_list_empty(&fdi->in))) {errno = EEXIST; return -1;}
    dill_pollset[fdi->idx].events |= POLLIN;
    dill_waitfor(cl, id, &fdi->in, NULL);
    return 0;
}

int dill_pollset_out(struct dill_clause *cl, int id, int fd) {
    struct dill_fdinfo *fdi = &dill_fdinfos[fd];
    if(dill_slow(!fdi->cached)) {
        int flags = fcntl(fd, F_GETFD);
        if(flags < 0 && errno == EBADF) return -1;
        dill_assert(flags >= 0);
        fdi->cached = 1;
    }
    if(fdi->idx < 0) {
        dill_assert(dill_pollset_size < dill_maxfds());
        fdi->idx = dill_pollset_size;
        ++dill_pollset_size;
        dill_pollset[fdi->idx].fd = fd;
    }
    if(dill_slow(!dill_list_empty(&fdi->out))) {errno = EEXIST; return -1;}
    dill_pollset[fdi->idx].events |= POLLOUT;
    dill_waitfor(cl, id, &fdi->out, NULL);
    return 0;
}

void dill_pollset_clean(int fd) {
    struct dill_fdinfo *fdi = &dill_fdinfos[fd];
    fdi->cached = 0;
}

int dill_pollset_poll(int timeout) {
    /* Wait for events. */
    int numevs;
    while(1) {
        numevs = poll(dill_pollset, dill_pollset_size, timeout);
        if(numevs < 0 && errno == EINTR)
            continue;
        dill_assert(numevs >= 0);
        break;  
    }
    int result = numevs > 0 ? 1 : 0;
    /* Fire file descriptor events as needed. */
    int i;
    for(i = 0; i != dill_pollset_size && numevs; ++i) {
        struct pollfd *pfd = &dill_pollset[i];
        struct dill_fdinfo *fdi = &dill_fdinfos[pfd->fd];
        /* Signal from parent process. */
        if(dill_slow(pfd->fd == dill_parent)) {
            dill_parent = -1;
            dill_shutdown();
            continue;
        }
        /* Resume the blocked coroutines. */
        if(!dill_list_empty(&fdi->in) &&
              pfd->revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) {
            pfd->events &= ~POLLIN;
            struct dill_clause *cl = dill_cont(dill_list_begin(&fdi->in),
                struct dill_clause, epitem);
            dill_trigger(cl, 0);
        }
        if(!dill_list_empty(&fdi->out) &&
              pfd->revents & (POLLOUT | POLLERR | POLLHUP | POLLNVAL)) {
            pfd->events &= ~POLLOUT;
            struct dill_clause *cl = dill_cont(dill_list_begin(&fdi->out),
                struct dill_clause, epitem);
            dill_trigger(cl, 0);
        }
        /* If nobody is polling for the fd remove it from the pollset. */
        if(!pfd->events) {
            fdi->idx = -1;
            dill_assert(dill_list_empty(&fdi->in) &&
                dill_list_empty(&fdi->out));
            --dill_pollset_size;
            /* Pollset has to be compact. Thus, unless we are removing the
               last item from the pollset we want to move the last item
               to the vacant slot left by the removed fd. */
            if(i != dill_pollset_size) {
                struct pollfd *lastpfd = &dill_pollset[dill_pollset_size];
                *pfd = *lastpfd;
                dill_fdinfos[pfd->fd].idx = i;
            }
            --i;
            --numevs;
        }
    }
    return result;
}

