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
#include <poll.h>
#include <stddef.h>
#include <stdlib.h>

#include "cr.h"
#include "list.h"
#include "utils.h"

/* Pollset used for waiting for file descriptors. */
static int ts_pollset_size = 0;
static int ts_pollset_capacity = 0;
static struct pollfd *ts_pollset_fds = NULL;

/* The item at a specific index in this array corresponds to the entry
   in ts_pollset fds with the same index. */
struct ts_pollset_item {
    struct ts_cr *in;
    struct ts_cr *out;
};
static struct ts_pollset_item *ts_pollset_items = NULL;

/* Find pollset index by fd. If fd is not in pollset, return the index after
   the last item. TODO: This is O(n) operation! */
static int ts_find_pollset(int fd) {
    int i;
    for(i = 0; i != ts_pollset_size; ++i) {
        if(ts_pollset_fds[i].fd == fd)
            break;
    }
    return i;
}

void ts_poller_init(void) {
    errno = 0;
}

pid_t ts_fork(void) {
}

static void ts_poller_add(int fd, int events) {
    int i = ts_find_pollset(fd);
    /* Grow the pollset as needed. */
    if(i == ts_pollset_size) {
        if(ts_pollset_size == ts_pollset_capacity) {
            ts_pollset_capacity = ts_pollset_capacity ?
                ts_pollset_capacity * 2 : 64;
            ts_pollset_fds = realloc(ts_pollset_fds,
                ts_pollset_capacity * sizeof(struct pollfd));
            ts_pollset_items = realloc(ts_pollset_items,
                ts_pollset_capacity * sizeof(struct ts_pollset_item));
        }
        ++ts_pollset_size;
        ts_pollset_fds[i].fd = fd;
        ts_pollset_fds[i].events = 0;
        ts_pollset_fds[i].revents = 0;
        ts_pollset_items[i].in = NULL;
        ts_pollset_items[i].out = NULL;
    }
    /* Register the new file descriptor in the pollset. */
    if(events & FDW_IN) {
        if(ts_slow(ts_pollset_items[i].in))
            ts_panic(
                "multiple coroutines waiting for a single file descriptor");
        ts_pollset_fds[i].events |= POLLIN;
        ts_pollset_items[i].in = ts_running;
    }
    if(events & FDW_OUT) {
        if(ts_slow(ts_pollset_items[i].out))
            ts_panic(
                "multiple coroutines waiting for a single file descriptor");
        ts_pollset_fds[i].events |= POLLOUT;
        ts_pollset_items[i].out = ts_running;
    }
}

static void ts_poller_rm(int fd, int events) {
    int i = ts_find_pollset(fd);
    ts_assert(i < ts_pollset_size);
    if(ts_pollset_items[i].in == ts_running) {
        ts_pollset_items[i].in = NULL;
        ts_pollset_fds[i].events &= ~POLLIN;
    }
    if(ts_pollset_items[i].out == ts_running) {
        ts_pollset_items[i].out = NULL;
        ts_pollset_fds[i].events &= ~POLLOUT;
    }
    if(!ts_pollset_fds[i].events) {
        --ts_pollset_size;
        if(i < ts_pollset_size) {
            ts_pollset_items[i] = ts_pollset_items[ts_pollset_size];
            ts_pollset_fds[i] = ts_pollset_fds[ts_pollset_size];
        }
    }
}

static void ts_poller_clean(int fd) {
}

static int ts_poller_wait(int timeout) {
    /* Wait for events. */
    int numevs;
    while(1) {
        numevs = poll(ts_pollset_fds, ts_pollset_size, timeout);
        if(numevs < 0 && errno == EINTR)
            continue;
        ts_assert(numevs >= 0);
        break;  
    }
    /* Fire file descriptor events. */
    int result = numevs > 0 ? 1 : 0;
    int i;
    for(i = 0; i != ts_pollset_size && numevs; ++i) {
        int inevents = 0;
        int outevents = 0;
        /* Set the result values. */
        if(ts_pollset_fds[i].revents & POLLIN)
            inevents |= FDW_IN;
        if(ts_pollset_fds[i].revents & POLLOUT)
            outevents |= FDW_OUT;
        if(ts_pollset_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            inevents |= FDW_ERR;
            outevents |= FDW_ERR;
        }
        /* Resume the blocked coroutines. */
        if(ts_pollset_items[i].in &&
              ts_pollset_items[i].in == ts_pollset_items[i].out) {
            struct ts_cr *cr = ts_pollset_items[i].in;
            ts_resume(cr, inevents | outevents);
            ts_pollset_fds[i].events = 0;
            ts_pollset_items[i].in = NULL;
            ts_pollset_items[i].out = NULL;
        }
        else {
            if(ts_pollset_items[i].in && inevents) {
                struct ts_cr *cr = ts_pollset_items[i].in;
                ts_resume(cr, inevents);
                ts_pollset_fds[i].events &= ~POLLIN;
                ts_pollset_items[i].in = NULL;
            }
            else if(ts_pollset_items[i].out && outevents) {
                struct ts_cr *cr = ts_pollset_items[i].out;
                ts_resume(cr, outevents);
                ts_pollset_fds[i].events &= ~POLLOUT;
                ts_pollset_items[i].out = NULL;
            }
        }
        /* If nobody is polling for the fd remove it from the pollset. */
        if(!ts_pollset_fds[i].events) {
            ts_assert(!ts_pollset_items[i].in &&
                !ts_pollset_items[i].out);
            --ts_pollset_size;
            if(i != ts_pollset_size) {
                ts_pollset_fds[i] = ts_pollset_fds[ts_pollset_size];
                ts_pollset_items[i] = ts_pollset_items[ts_pollset_size];
            }
            --i;
            --numevs;
        }
    }
    return result;
}

