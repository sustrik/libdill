/*

  Copyright (c) 2018 Martin Sustrik

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

#include <error.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdlib.h>

#include "fd.h"
#include "utils.h"

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"

DILL_UNIQUE_ID(dill_tcp_type)

const struct dill_tcp_opts dill_tcp_defaults = {
    NULL,  /* mem */
    0,     /* rx_buffer */
    0      /* nodelay */
};

struct dill_tcp {
    struct dill_hvfs hvfs;
    struct dill_bsock_vfs bvfs;
    struct dill_tcp_opts opts;
    int fd;
    unsigned int busy : 1;
    unsigned int indone : 1;
    unsigned int outdone: 1;
};

DILL_CHECK_STORAGE(dill_tcp, dill_tcp_storage)

static void *dill_tcp_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_tcp *self = (struct dill_tcp*)hvfs;
    if(type == dill_bsock_type) return &self->bvfs;
    if(type == dill_tcp_type) return self;
    errno = ENOTSUP;
    return NULL;
}

static void dill_tcp_hclose(struct dill_hvfs *hvfs) {
    struct dill_tcp *self = (struct dill_tcp*)hvfs;
    if(self->fd >= 0) dill_fd_close(self->fd);
    //if(self->rx_buffering) dill_fd_termrxbuf(&self->rxbuf);
    if(!self->opts.mem) free(self);
}

static int dill_tcp_bsend(struct dill_bsock_vfs *bvfs,
      const void *buf, size_t len, int64_t deadline) {
    dill_assert(0);
}

static int dill_tcp_brecv(struct dill_bsock_vfs *bvfs,
      void *buf, size_t len, int64_t deadline) {
    dill_assert(0);
}

static int dill_tcp_create(int fd, const struct dill_tcp_opts *opts) {
    int err;
    if(opts->nodelay) {
        /* Switch off Nagle's algorithm. */
        int val = 1;
        int rc = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
        if(dill_slow(rc < 0)) {err = errno; goto error1;}
    }
    struct dill_tcp *self = (struct dill_tcp*)opts->mem;
    if(!self) {
        self = malloc(sizeof(struct dill_tcp));
        if(dill_slow(!self)) {err = ENOMEM; goto error1;}
    }
    self->hvfs.query = dill_tcp_hquery;
    self->hvfs.close = dill_tcp_hclose;
    self->bvfs.bsend = dill_tcp_bsend;
    self->bvfs.brecv = dill_tcp_brecv;
    self->opts = *opts;
    self->fd = fd;
    self->busy = 0;
    self->indone = 0;
    self->outdone = 0;
    int h = dill_hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    if(!opts->mem) free(self);
error1:
    errno = err;
    return -1;
}

int dill_tcp_connect(const struct dill_ipaddr *addr,
      const struct dill_tcp_opts *opts, int64_t deadline) {
    int err;
    if(!opts) opts = &dill_tcp_defaults;
    int s = socket(dill_ipaddr_family(addr), SOCK_STREAM, 0);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    int rc = dill_fd_tune(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    rc = dill_fd_connect(s, dill_ipaddr_sockaddr(addr), dill_ipaddr_len(addr),
        deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    int h = dill_tcp_create(s, opts);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    dill_fd_close(s);
error1:
    errno = err;
    return -1;
}

int dill_tcp_accept(int s, const struct dill_tcp_opts *opts,
      struct dill_ipaddr *addr, int64_t deadline) {
    int fd = dill_tcp_listener_acceptfd(s, addr, deadline);
    if(dill_slow(fd < 0)) return -1;
    return dill_tcp_fromfd(fd, opts);
}

int dill_tcp_fromfd(int fd, const struct dill_tcp_opts *opts) {
    int err;
    if(!opts) opts = &dill_tcp_defaults;
    int rc = dill_fd_check(fd, SOCK_STREAM, AF_INET, AF_INET6, 0);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    if(dill_slow(rc == 0)) {err = EINVAL; goto error1;}
    fd = dill_fd_own(fd);
    if(dill_slow(fd < 0)) {err = errno; goto error1;}
    rc = dill_fd_tune(fd);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    int h = dill_tcp_create(fd, opts);
    if(dill_slow(h < 0)) {err = errno; goto error1;}
    return h;
error1:
    if(err != EBADF) dill_fd_close(fd);
    errno = err;
    return -1;
}

int dill_tcp_tofd(int s) {
    int err;
    struct dill_tcp *self = dill_hquery(s, dill_tcp_type);
    if(dill_slow(!self)) {err = errno; goto error1;}
    // TODO: If there's a rx buffer, return an error.
    int fd = self->fd;
    self->fd = -1;
    dill_tcp_hclose(&self->hvfs);
    return fd;
error1:
    dill_hclose(s);
    errno = err;
    return -1;
}
