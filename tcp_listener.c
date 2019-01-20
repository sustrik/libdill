/*

  Copyright (c) 2019 Martin Sustrik

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

DILL_UNIQUE_ID(dill_tcp_listener_type)

const struct dill_tcp_listener_opts dill_tcp_listener_defaults = {
    NULL,  /* mem */
    10,    /* backlog */
};

struct dill_tcp_listener {
    struct dill_hvfs hvfs;
    struct dill_tcp_listener_opts opts;
    int fd;
};

DILL_CHECK_STORAGE(dill_tcp_listener, dill_tcp_listener_storage)

static void *dill_tcp_listener_hquery(struct dill_hvfs *hvfs,
      const void *type) {
    struct dill_tcp_listener *self = (struct dill_tcp_listener*)hvfs;
    if(type == dill_tcp_listener_type) return self;
    errno = ENOTSUP;
    return NULL;
}

static void dill_tcp_listener_hclose(struct dill_hvfs *hvfs) {
    struct dill_tcp_listener *self = (struct dill_tcp_listener*)hvfs;
    dill_fd_close(self->fd);
    if(!self->opts.mem) free(self);
}

static int dill_tcp_listener_create(int fd,
      const struct dill_tcp_listener_opts *opts) {
    int err;
    struct dill_tcp_listener *self = (struct dill_tcp_listener*)opts->mem;
    if(!self) {
        self = malloc(sizeof(struct dill_tcp_listener));
        if(dill_slow(!self)) {err = ENOMEM; goto error1;}
    }
    self->hvfs.query = dill_tcp_listener_hquery;
    self->hvfs.close = dill_tcp_listener_hclose;
    self->opts = *opts;
    self->fd = fd;
    int h = dill_hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    if(!opts->mem) free(self);
error1:
    errno = err;
    return -1;
}

int dill_tcp_listener_make(struct dill_ipaddr *addr,
      const struct dill_tcp_listener_opts *opts) {
    int err;
    if(!opts) opts = &dill_tcp_listener_defaults;
    int s = socket(dill_ipaddr_family(addr), SOCK_STREAM, 0);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    int rc = dill_fd_tune(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    rc = bind(s, dill_ipaddr_sockaddr(addr), dill_ipaddr_len(addr));
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    rc = listen(s, opts->backlog);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS. */
    if(dill_ipaddr_port(addr) == 0) {
        struct dill_ipaddr baddr;
        socklen_t len = sizeof(struct dill_ipaddr);
        rc = getsockname(s, (struct sockaddr*)&baddr, &len);
        if(dill_slow(rc < 0)) {err = errno; goto error2;}
        dill_ipaddr_setport(addr, dill_ipaddr_port(&baddr));
    }
    int h = dill_tcp_listener_create(s, opts);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    close(s);
error1:
    errno = err;
    return -1;
}

int dill_tcp_listener_fromfd(int fd,
      const struct dill_tcp_listener_opts *opts) {
    int err;
    if(!opts) opts = &dill_tcp_listener_defaults;
    int rc = dill_fd_check(fd, SOCK_STREAM, AF_INET, AF_INET6, 1);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    if(dill_slow(rc == 0)) {err = EINVAL; goto error1;}
    fd = dill_fd_own(fd);
    if(dill_slow(fd < 0)) {err = errno; goto error1;}
    rc = dill_fd_tune(fd);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    int h = dill_tcp_listener_create(fd, opts);
    if(dill_slow(h < 0)) {err = errno; goto error1;}
    return h;
error1:
    if(err != EBADF) dill_fd_close(fd);
    errno = err;
    return -1;
}

int dill_tcp_listener_tofd(int s) {
    int err;
    struct dill_tcp_listener *self = dill_hquery(s, dill_tcp_listener_type);
    if(dill_slow(!self)) {err = errno; goto error1;}
    int fd = self->fd;
    self->fd = -1;
    dill_tcp_listener_hclose(&self->hvfs);
    return fd;
error1:
    dill_hclose(s);
    errno = err;
    return -1;
}

int dill_tcp_listener_acceptfd(int s, struct dill_ipaddr *addr,
      int64_t deadline) {
    struct dill_tcp_listener *self = dill_hquery(s, dill_tcp_listener_type);
    if(dill_slow(!self)) return -1;
    socklen_t addrlen = sizeof(struct dill_ipaddr);
    int as = dill_fd_accept(self->fd, (struct sockaddr*)addr, &addrlen,
        deadline);
    /* TODO: Should this nullify the socket? */
    if(dill_slow(as < 0)) return -1;
    return as;
}

