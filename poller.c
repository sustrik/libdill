/*

  Copyright (c) 2017 Martin Sustrik

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
#include <stdlib.h>

#include "fd.h"
#include "poller.h"
#include "utils.h"

int dill_poller_init(struct dill_poller *self) {
    int err;
    int rc = dill_apoll_init(&self->apoll);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    self->nfds = dill_maxfds();
    self->fds = calloc(sizeof(struct dill_pollerfd), self->nfds);
    if(dill_slow(!self->fds)) {err = ENOMEM; goto error2;}
    dill_list_init(&self->diff);
    return 0;
error2:
    dill_apoll_term(&self->apoll);
error1:
    errno = err;
    return -1;
}

void dill_poller_term(struct dill_poller *self) {
    free(self->fds);
    dill_apoll_term(&self->apoll);
}

int dill_poller_in(struct dill_poller *self, int fd) {
    if(dill_slow(fd < 0 || fd >= self->nfds)) {errno = EBADF; return -1;}
    struct dill_pollerfd *pfd = &self->fds[fd];
    pfd->curr |= DILL_IN;
    if(!pfd->item.prev) dill_list_insert(&pfd->item, &self->diff);
}

int dill_poller_out(struct dill_poller *self, int fd) {
    if(dill_slow(fd < 0 || fd >= self->nfds)) {errno = EBADF; return -1;}
    struct dill_pollerfd *pfd = &self->fds[fd];
    pfd->curr |= DILL_OUT;
    if(!pfd->item.prev) dill_list_insert(&pfd->item, &self->diff);
}

int dill_poller_clean(struct dill_poller *self, int fd) {
    if(dill_slow(fd < 0 || fd >= self->nfds)) {errno = EBADF; return -1;}
    struct dill_pollerfd *pfd = &self->fds[fd];
    dill_apoll_clean(&self->apoll, fd, pfd->old);
    pfd->curr = 0;
    pfd->old = 0;
    if(pfd->item.prev) dill_list_erase(&pfd->item);
    return 0;
}

int dill_poller_event(struct dill_poller *self, int *fd, int *event,
      int timeout) {
    /* Get an event from apoll. Ignore any events that the user is no longer
       polling for. */
    while(1) {
        int rc = dill_apoll_event(&self->apoll, fd, event);
        if(rc < 0) break;
        if(*event & self->fds[*fd].curr) return 0;
    }
    /* There are no more events to be reported from apoll.
       Let's adjust the pollset and poll again. */
    struct dill_list *it = dill_list_next(&self->diff);
    while(it != &self->diff) {
        struct dill_pollerfd *pfd = dill_cont(it, struct dill_pollerfd, item);
        if(pfd->curr != pfd->old)
            dill_apoll_ctl(&self->apoll, pfd - self->fds, pfd->curr, pfd->old);
        it->prev = NULL;
        it = dill_list_next(it);
    }
    dill_list_init(&self->diff);
    int rc = dill_apoll_wait(&self->apoll, timeout);
    if(dill_slow(rc < 0)) return -1;
    /* Now there should be at least one event in apoll. */
    rc = dill_apoll_event(&self->apoll, fd, event);
    dill_assert(rc >= 0);
    return 0;
}

