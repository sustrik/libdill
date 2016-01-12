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
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>

#include "cr.h"
#include "list.h"
#include "utils.h"

/* Pollset used for waiting for file descriptors. */
static int dill_pollset_size = 0;
static int dill_pollset_capacity = 0;
static struct pollfd *dill_pollset_fds = NULL;

/* The item at a specific index in this array corresponds to the entry
   in dill_pollset fds with the same index. */
struct dill_pollset_item {
    struct dill_cr *in;
    struct dill_cr *out;
};
static struct dill_pollset_item *dill_pollset_items = NULL;

/* Find pollset index by fd. If fd is not in pollset, return the index after
   the last item. TODO: This is O(n) operation! */
static int dill_find_pollset(int fd) {
    int i;
    for(i = 0; i != dill_pollset_size; ++i) {
        if(dill_pollset_fds[i].fd == fd)
            break;
    }
    return i;
}

void dill_poller_init(void) {
    errno = 0;
}

pid_t dill_fork(void) {
}

static void dill_poller_add(int fd, int events) {
    int i = dill_find_pollset(fd);
    /* Grow the pollset as needed. */
    if(i == dill_pollset_size) {
        if(dill_pollset_size == dill_pollset_capacity) {
            dill_pollset_capacity = dill_pollset_capacity ?
                dill_pollset_capacity * 2 : 64;
            dill_pollset_fds = realloc(dill_pollset_fds,
                dill_pollset_capacity * sizeof(struct pollfd));
            dill_pollset_items = realloc(dill_pollset_items,
                dill_pollset_capacity * sizeof(struct dill_pollset_item));
        }
        ++dill_pollset_size;
        dill_pollset_fds[i].fd = fd;
        dill_pollset_fds[i].events = 0;
        dill_pollset_fds[i].revents = 0;
        dill_pollset_items[i].in = NULL;
        dill_pollset_items[i].out = NULL;
    }
    /* Register the new file descriptor in the pollset. */
    if(events & FDW_IN) {
        if(dill_slow(dill_pollset_items[i].in))
            dill_panic(
                "multiple coroutines waiting for a single file descriptor");
        dill_pollset_fds[i].events |= POLLIN;
        dill_pollset_items[i].in = dill_running;
    }
    if(events & FDW_OUT) {
        if(dill_slow(dill_pollset_items[i].out))
            dill_panic(
                "multiple coroutines waiting for a single file descriptor");
        dill_pollset_fds[i].events |= POLLOUT;
        dill_pollset_items[i].out = dill_running;
    }
}

static void dill_poller_rm(int fd, int events) {
    int i = dill_find_pollset(fd);
    dill_assert(i < dill_pollset_size);
    if(dill_pollset_items[i].in == dill_running) {
        dill_pollset_items[i].in = NULL;
        dill_pollset_fds[i].events &= ~POLLIN;
    }
    if(dill_pollset_items[i].out == dill_running) {
        dill_pollset_items[i].out = NULL;
        dill_pollset_fds[i].events &= ~POLLOUT;
    }
    if(!dill_pollset_fds[i].events) {
        --dill_pollset_size;
        if(i < dill_pollset_size) {
            dill_pollset_items[i] = dill_pollset_items[dill_pollset_size];
            dill_pollset_fds[i] = dill_pollset_fds[dill_pollset_size];
        }
    }
}

static void dill_poller_clean(int fd) {
}

static int dill_poller_wait(int timeout) {
    /* Wait for events. */
    int numevs;
    while(1) {
        numevs = poll(dill_pollset_fds, dill_pollset_size, timeout);
        if(numevs < 0 && errno == EINTR)
            continue;
        dill_assert(numevs >= 0);
        break;  
    }
    /* Fire file descriptor events. */
    int result = numevs > 0 ? 1 : 0;
    int i;
    for(i = 0; i != dill_pollset_size && numevs; ++i) {
        int inevents = 0;
        int outevents = 0;
        /* Set the result values. */
        if(dill_pollset_fds[i].revents & POLLIN)
            inevents |= FDW_IN;
        if(dill_pollset_fds[i].revents & POLLOUT)
            outevents |= FDW_OUT;
        if(dill_pollset_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            inevents |= FDW_ERR;
            outevents |= FDW_ERR;
        }
        /* Resume the blocked coroutines. */
        if(dill_pollset_items[i].in &&
              dill_pollset_items[i].in == dill_pollset_items[i].out) {
            struct dill_cr *cr = dill_pollset_items[i].in;
            dill_resume(cr, inevents | outevents);
            dill_pollset_fds[i].events = 0;
            dill_pollset_items[i].in = NULL;
            dill_pollset_items[i].out = NULL;
        }
        else {
            if(dill_pollset_items[i].in && inevents) {
                struct dill_cr *cr = dill_pollset_items[i].in;
                dill_resume(cr, inevents);
                dill_pollset_fds[i].events &= ~POLLIN;
                dill_pollset_items[i].in = NULL;
            }
            else if(dill_pollset_items[i].out && outevents) {
                struct dill_cr *cr = dill_pollset_items[i].out;
                dill_resume(cr, outevents);
                dill_pollset_fds[i].events &= ~POLLOUT;
                dill_pollset_items[i].out = NULL;
            }
        }
        /* If nobody is polling for the fd remove it from the pollset. */
        if(!dill_pollset_fds[i].events) {
            dill_assert(!dill_pollset_items[i].in &&
                !dill_pollset_items[i].out);
            --dill_pollset_size;
            if(i != dill_pollset_size) {
                dill_pollset_fds[i] = dill_pollset_fds[dill_pollset_size];
                dill_pollset_items[i] = dill_pollset_items[dill_pollset_size];
            }
            --i;
            --numevs;
        }
    }
    return result;
}

