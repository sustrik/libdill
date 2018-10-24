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
#include <unistd.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "fd.h"
#include "utils.h"

dill_unique_id(dill_tcp_type);
dill_unique_id(dill_tcp_listener_type);

/******************************************************************************/
/*  TCP connection socket                                                     */
/******************************************************************************/

static void *dill_tcp_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_tcp_hclose(struct dill_hvfs *hvfs);
static int dill_tcp_bsendl(struct dill_bsock_vfs *bvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
static int dill_tcp_brecvl(struct dill_bsock_vfs *bvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);

struct dill_tcp_conn {
    struct dill_hvfs hvfs;
    struct dill_bsock_vfs bvfs;
    int fd;
    struct dill_fd_rxbuf rxbuf;
    unsigned int rbusy : 1;
    unsigned int sbusy : 1;
    unsigned int indone : 1;
    unsigned int outdone: 1;
    unsigned int inerr : 1;
    unsigned int outerr : 1;
    unsigned int mem : 1;
};

DILL_CHECK_STORAGE(dill_tcp_conn, dill_tcp_storage)

static void *dill_tcp_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_tcp_conn *self = (struct dill_tcp_conn*)hvfs;
    if(type == dill_bsock_type) return &self->bvfs;
    if(type == dill_tcp_type) return self;
    errno = ENOTSUP;
    return NULL;
}

static int dill_tcp_makeconn(int fd, void *mem) {
    /* Create the object. */
    struct dill_tcp_conn *self = (struct dill_tcp_conn*)mem;
    self->hvfs.query = dill_tcp_hquery;
    self->hvfs.close = dill_tcp_hclose;
    self->bvfs.bsendl = dill_tcp_bsendl;
    self->bvfs.brecvl = dill_tcp_brecvl;
    self->fd = fd;
    dill_fd_initrxbuf(&self->rxbuf);
    self->rbusy = 0;
    self->sbusy = 0;
    self->indone = 0;
    self->outdone = 0;
    self->inerr = 0;
    self->outerr = 0;
    self->mem = 1;
    /* Create the handle. */
    return dill_hmake(&self->hvfs);
}

int dill_tcp_fromfd_mem(int fd, struct dill_tcp_storage *mem) {
    int err;
    if(dill_slow(!mem || fd < 0)) {err = EINVAL; goto error1;}
    /* Make sure that the supplied file descriptor is of correct type. */
    int rc = dill_fd_check(fd, SOCK_STREAM, AF_INET, AF_INET6, 0);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    /* Take ownership of the file descriptor. */
    fd = dill_fd_own(fd);
    if(dill_slow(fd < 0)) {err = errno; goto error1;}
    /* Set the socket to non-blocking mode */
    rc = dill_fd_unblock(fd);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    /* Create the handle */
    int h = dill_tcp_makeconn(fd, mem);
    if(dill_slow(h < 0)) {err = errno; goto error1;}
    /* Return the handle */
    return h;
error1:
    errno = err;
    return -1;
}

int dill_tcp_fromfd(int fd) {
    int err;
    struct dill_tcp_conn *obj = malloc(sizeof(struct dill_tcp_conn));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int s = dill_tcp_fromfd_mem(fd, (struct dill_tcp_storage*)obj);
    if (dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int dill_tcp_connect_mem(const struct dill_ipaddr *addr,
        struct dill_tcp_storage *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* Open a socket. */
    int s = socket(dill_ipaddr_family(addr), SOCK_STREAM, 0);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dill_fd_unblock(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Connect to the remote endpoint. */
    rc = dill_fd_connect(s, dill_ipaddr_sockaddr(addr), dill_ipaddr_len(addr),
        deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = dill_tcp_makeconn(s, mem);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    dill_fd_close(s);
error1:
    errno = err;
    return -1;
}

int dill_tcp_connect(const struct dill_ipaddr *addr, int64_t deadline) {
    int err;
    struct dill_tcp_conn *obj = malloc(sizeof(struct dill_tcp_conn));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int s = dill_tcp_connect_mem(addr, (struct dill_tcp_storage*)obj, deadline);
    if(dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    errno = err;
    return -1;

}

static int dill_tcp_bsendl(struct dill_bsock_vfs *bvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_tcp_conn *self = dill_cont(bvfs, struct dill_tcp_conn, bvfs);
    if(dill_slow(self->sbusy)) {errno = EBUSY; return -1;}
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    self->sbusy = 1;
    ssize_t sz = dill_fd_send(self->fd, first, last, deadline);
    self->sbusy = 0;
    if(dill_fast(sz >= 0)) return sz;
    self->outerr = 1;
    return -1;
}

static int dill_tcp_brecvl(struct dill_bsock_vfs *bvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_tcp_conn *self = dill_cont(bvfs, struct dill_tcp_conn, bvfs);
    if(dill_slow(self->rbusy)) {errno = EBUSY; return -1;}
    if(dill_slow(self->indone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->inerr)) {errno = ECONNRESET; return -1;}
    self->rbusy = 1;
    int rc = dill_fd_recv(self->fd, &self->rxbuf, first, last, deadline);
    self->rbusy = 0;
    if(dill_fast(rc == 0)) return 0;
    if(errno == EPIPE) self->indone = 1;
    else self->inerr = 1;
    return -1;
}

int dill_tcp_done(int s, int64_t deadline) {
    struct dill_tcp_conn *self = dill_hquery(s, dill_tcp_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    /* Flushing the tx buffer is done asynchronously on kernel level. */
    int rc = shutdown(self->fd, SHUT_WR);
    if(dill_slow(rc < 0)) {
        if(errno == ENOTCONN) {self->outerr = 1; errno = ECONNRESET; return -1;}
        if(errno == ENOBUFS) {self->outerr = 1; errno = ENOMEM; return -1;}
        dill_assert(rc == 0);
    }
    self->outdone = 1;
    return 0;
}

int dill_tcp_close(int s, int64_t deadline) {
    int err;
    /* Listener socket needs no special treatment. */
    if(dill_hquery(s, dill_tcp_listener_type)) {
        return dill_hclose(s);
    }
    struct dill_tcp_conn *self = dill_hquery(s, dill_tcp_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->inerr || self->outerr)) {err = ECONNRESET; goto error;}
    /* If not done already, flush the outbound data and start the terminal
       handshake. */
    if(!self->outdone) {
        int rc = dill_tcp_done(s, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Now we are going to read all the inbound data until we reach end of the
       stream. That way we can be sure that the peer either received all our
       data or consciously closed the connection without reading all of it. */
    int rc = dill_tcp_brecvl(&self->bvfs, NULL, NULL, deadline);
    dill_assert(rc < 0);
    if(dill_slow(errno != EPIPE)) {err = errno; goto error;}
    dill_tcp_hclose(&self->hvfs);
    return 0;
error:
    dill_tcp_hclose(&self->hvfs);
    errno = err;
    return -1;
}

static void dill_tcp_hclose(struct dill_hvfs *hvfs) {
    struct dill_tcp_conn *self = (struct dill_tcp_conn*)hvfs;
    dill_fd_close(self->fd);
    dill_fd_termrxbuf(&self->rxbuf);
    if(!self->mem) free(self);
}

/******************************************************************************/
/*  TCP listener socket                                                       */
/******************************************************************************/

static void *dill_tcp_listener_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_tcp_listener_hclose(struct dill_hvfs *hvfs);

struct dill_tcp_listener {
    struct dill_hvfs hvfs;
    int fd;
    struct dill_ipaddr addr;
    unsigned int mem : 1;
};

DILL_CHECK_STORAGE(dill_tcp_listener, dill_tcp_listener_storage)

static int dill_tcp_makelistener(int fd, void *mem) {
    /* Create the object. */
    struct dill_tcp_listener *self = (struct dill_tcp_listener*)mem;
    self->hvfs.query = dill_tcp_listener_hquery;
    self->hvfs.close = dill_tcp_listener_hclose;
    self->fd = fd;
    self->mem = 1;
    /* Create the handle. */
    return dill_hmake(&self->hvfs);
}

static void *dill_tcp_listener_hquery(struct dill_hvfs *hvfs,
      const void *type) {
    struct dill_tcp_listener *self = (struct dill_tcp_listener*)hvfs;
    if(type == dill_tcp_listener_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int dill_tcp_listener_fromfd(int fd) {
    int err;
    struct dill_tcp_listener *obj = malloc(sizeof(struct dill_tcp_listener));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int s = dill_tcp_listener_fromfd_mem(fd,
        (struct dill_tcp_listener_storage*)obj);
    if (dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int dill_tcp_listener_fromfd_mem(int fd,
      struct dill_tcp_listener_storage *mem) {
    int err;
    if(dill_slow(!mem || fd < 0)) {err = EINVAL; goto error1;}
    /* Make sure that the supplied file descriptor is of correct type. */
    int rc = dill_fd_check(fd, SOCK_STREAM, AF_INET, AF_INET6, 1);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    /* Take ownership of the file descriptor. */
    fd = dill_fd_own(fd);
    if(dill_slow(fd < 0)) {err = errno; goto error1;}
    /* Set the socket to non-blocking mode */
    rc = dill_fd_unblock(fd);
    if(dill_slow(rc < 0)) {err = errno; goto error1;}
    /* Create the handle */
    int h = dill_tcp_makelistener(fd, mem);
    if(dill_slow(h < 0)) {err = errno; goto error1;}
    /* Return the handle */
    return h;
error1:
    errno = err;
    return -1;
}

int dill_tcp_listen_mem(struct dill_ipaddr *addr, int backlog,
      struct dill_tcp_listener_storage *mem) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* Open the listening socket. */
    int s = socket(dill_ipaddr_family(addr), SOCK_STREAM, 0);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dill_fd_unblock(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Start listening for incoming connections. */
    rc = bind(s, dill_ipaddr_sockaddr(addr), dill_ipaddr_len(addr));
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    rc = listen(s, backlog);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS. */
    if(dill_ipaddr_port(addr) == 0) {
        struct dill_ipaddr baddr;
        socklen_t len = sizeof(struct dill_ipaddr);
        rc = getsockname(s, (struct sockaddr*)&baddr, &len);
        if(rc < 0) {err = errno; goto error2;}
        dill_ipaddr_setport(addr, dill_ipaddr_port(&baddr));
    }
    int h = dill_tcp_makelistener(s, mem);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    close(s);
error1:
    errno = err;
    return -1;
}

int dill_tcp_listen(struct dill_ipaddr *addr, int backlog) {
    int err;
    struct dill_tcp_listener *obj = malloc(sizeof(struct dill_tcp_listener));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int ls = dill_tcp_listen_mem(addr, backlog,
        (struct dill_tcp_listener_storage*)obj);
    if(dill_slow(ls < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return ls;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int dill_tcp_accept_mem(int s, struct dill_ipaddr *addr,
        struct dill_tcp_storage *mem, int64_t deadline) {
    int err;
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    /* Retrieve the listener object. */
    struct dill_tcp_listener *lst = dill_hquery(s, dill_tcp_listener_type);
    if(dill_slow(!lst)) {err = errno; goto error1;}
    /* Try to get new connection in a non-blocking way. */
    socklen_t addrlen = sizeof(struct dill_ipaddr);
    int as = dill_fd_accept(lst->fd, (struct sockaddr*)addr, &addrlen,
        deadline);
    if(dill_slow(as < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dill_fd_unblock(as);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = dill_tcp_makeconn(as, mem);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    dill_fd_close(as);
error1:
    errno = err;
    return -1;
}

int dill_tcp_accept(int s, struct dill_ipaddr *addr, int64_t deadline) {
    int err;
    struct dill_tcp_conn *obj = malloc(sizeof(struct dill_tcp_conn));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int as = dill_tcp_accept_mem(s, addr, (struct dill_tcp_storage*)obj,
        deadline);
    if(dill_slow(as < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return as;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

static void dill_tcp_listener_hclose(struct dill_hvfs *hvfs) {
    struct dill_tcp_listener *self = (struct dill_tcp_listener*)hvfs;
    dill_fd_close(self->fd);
    if(!self->mem) free(self);
}

