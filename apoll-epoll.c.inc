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
#include <unistd.h>

#include "apoll.h"
#include "utils.h"

int dill_apoll_init(struct dill_apoll *self) {
    self->efd = epoll_create(1);
    self->numevs = 0;
    self->pos = 0;
    if(dill_slow(self->efd < 0)) return -1;
    return 0;
}

void dill_apoll_term(struct dill_apoll *self) {
    int rc = close(self->efd);
    dill_assert(rc == 0);
}

void dill_apoll_ctl(struct dill_apoll *self, int fd, int events, int old) {
    int op;
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = 0;
    if(old == 0) {
        op = EPOLL_CTL_ADD;
    }
    else if(events == 0) {
        op = EPOLL_CTL_DEL;
    }
    else {
        op = EPOLL_CTL_MOD;
        if(events & DILL_IN) ev.events |= EPOLLIN;
        if(events & DILL_OUT) ev.events |= EPOLLOUT;
    }
    int rc = epoll_ctl(self->efd, op, fd, &ev);
    dill_assert(rc == 0);
}

int dill_apoll_poll(struct dill_apoll *self, int timeout) {
    self->numevs = epoll_wait(self->efd, self->events, DILL_EPOLLSETSIZE,
        timeout);
    if(dill_slow(self->numevs < 0 && errno == EINTR)) {
        self->numevs = 0; return -1;}
    dill_assert(self->numevs >= 0);
    if(self->numevs == 0) {errno = ETIMEDOUT; return -1;}
    self->pos = 0;
    return 0;
}

int dill_apoll_event(struct dill_apoll *self, int *fd, int *events) {
    if(self->pos >= self->numevs) return -1;
    struct epoll_event *ev = &self->events[self->pos];
    *fd = ev->data.fd;
    *events = 0;
    if(ev->events & (EPOLLIN | EPOLLERR | EPOLLHUP)) *events |= DILL_IN;
    if(ev->events & (EPOLLOUT | EPOLLERR | EPOLLHUP)) *events |= DILL_OUT;
    ++self->pos;
    return 0;
}

