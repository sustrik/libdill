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

#include "libdillimpl.h"
#include "fd.h"
#include "utils.h"

/* Secretly export the symbols. More thinking should be done about how
   to do this cleanly without breaking encapsulation. */
DILL_EXPORT extern const void *tcp_type;
DILL_EXPORT extern const void *tcp_listener_type;
DILL_EXPORT int tcp_fd(int s);

static int tcp_makeconn(int fd);

/******************************************************************************/
/*  TCP connection socket                                                     */
/******************************************************************************/

dill_unique_id(tcp_type);

static void *tcp_hquery(struct hvfs *hvfs, const void *type);
static void tcp_hclose(struct hvfs *hvfs);
static int tcp_hdone(struct hvfs *hvfs, int64_t deadline);
static int tcp_bsendl(struct bsock_vfs *bvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t tcp_brecvl(struct bsock_vfs *bvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

struct tcp_conn {
    struct hvfs hvfs;
    struct bsock_vfs bvfs;
    int fd;
    struct fd_rxbuf rxbuf;
    unsigned int indone : 1;
    unsigned int outdone: 1;
    unsigned int inerr : 1;
    unsigned int outerr : 1;
};

static void *tcp_hquery(struct hvfs *hvfs, const void *type) {
    struct tcp_conn *self = (struct tcp_conn*)hvfs;
    if(type == bsock_type) return &self->bvfs;
    if(type == tcp_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int tcp_connect(const struct ipaddr *addr, int64_t deadline) {
    int err;
    /* Open a socket. */
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fd_unblock(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Connect to the remote endpoint. */
    rc = fd_connect(s, ipaddr_sockaddr(addr), ipaddr_len(addr), deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = tcp_makeconn(s);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    fd_close(s);
error1:
    errno = err;
    return -1;
}

static int tcp_bsendl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct tcp_conn *self = dill_cont(bvfs, struct tcp_conn, bvfs);
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    ssize_t sz = fd_send(self->fd, first, last, deadline);
    if(dill_fast(sz >= 0)) return sz;
    self->outerr = 1;
    return -1;
}

static ssize_t tcp_brecvl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct tcp_conn *self = dill_cont(bvfs, struct tcp_conn, bvfs);
    if(dill_slow(self->indone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->inerr)) {errno = ECONNRESET; return -1;}
    int sz = fd_recv(self->fd, &self->rxbuf, first, last, deadline);
    if(dill_fast(sz > 0)) return sz;
    if(errno == EPIPE) self->indone = 1;
    else self->inerr = 1;
    return -1;
}

static int tcp_hdone(struct hvfs *hvfs, int64_t deadline) {
    struct tcp_conn *self = (struct tcp_conn*)hvfs;
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

int tcp_close(int s, int64_t deadline) {
    int err;
    struct tcp_conn *self = hquery(s, tcp_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->inerr || self->outerr)) {err = ECONNRESET; goto error;}
    /* If not done already, flush the outbound data and start the terminal
       handshake. */
    if(!self->outdone) {
        int rc = tcp_hdone(&self->hvfs, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Now we are going to read all the inbound data until we reach end of the
       stream. That way we can be sure that the peer either received all our
       data or consciously closed the connection without reading all of it. */
    int rc = tcp_brecvl(&self->bvfs, NULL, NULL, deadline);
    dill_assert(rc < 0);
    if(dill_slow(errno != EPIPE)) {err = errno; goto error;}
    return 0;
error:
    tcp_hclose(&self->hvfs);
    errno = err;
    return -1;
}

static void tcp_hclose(struct hvfs *hvfs) {
    struct tcp_conn *self = (struct tcp_conn*)hvfs;
    fd_close(self->fd);
    free(self);
}

/******************************************************************************/
/*  TCP listener socket                                                       */
/******************************************************************************/

dill_unique_id(tcp_listener_type);

static void *tcp_listener_hquery(struct hvfs *hvfs, const void *type);
static void tcp_listener_hclose(struct hvfs *hvfs);

struct tcp_listener {
    struct hvfs hvfs;
    int fd;
    struct ipaddr addr;
};

static void *tcp_listener_hquery(struct hvfs *hvfs, const void *type) {
    struct tcp_listener *self = (struct tcp_listener*)hvfs;
    if(type == tcp_listener_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int tcp_listen(struct ipaddr *addr, int backlog) {
    int err;
    /* Open the listening socket. */
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fd_unblock(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Start listening for incoming connections. */
    rc = bind(s, ipaddr_sockaddr(addr), ipaddr_len(addr));
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    rc = listen(s, backlog);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* If the user requested an ephemeral port,
       retrieve the port number assigned by the OS. */
    if(ipaddr_port(addr) == 0) {
        struct ipaddr baddr;
        socklen_t len = sizeof(struct ipaddr);
        rc = getsockname(s, (struct sockaddr*)&baddr, &len);
        if(rc < 0) {err = errno; goto error2;}
        ipaddr_setport(addr, ipaddr_port(&baddr));
    }
    /* Create the object. */
    struct tcp_listener *self = malloc(sizeof(struct tcp_listener));
    if(dill_slow(!self)) {err = ENOMEM; goto error2;}
    self->hvfs.query = tcp_listener_hquery;
    self->hvfs.close = tcp_listener_hclose;
    self->hvfs.done = NULL;
    self->fd = s;
    /* Create handle. */
    int h = hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    free(self);
error2:
    close(s);
error1:
    errno = err;
    return -1;
}

int tcp_accept(int s, struct ipaddr *addr, int64_t deadline) {
    int err;
    /* Retrieve the listener object. */
    struct tcp_listener *lst = hquery(s, tcp_listener_type);
    if(dill_slow(!lst)) {err = errno; goto error1;}
    /* Try to get new connection in a non-blocking way. */
    socklen_t addrlen = sizeof(struct ipaddr);
    int as = fd_accept(lst->fd, (struct sockaddr*)addr, &addrlen, deadline);
    if(dill_slow(as < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fd_unblock(as);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = tcp_makeconn(as);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    fd_close(as);
error1:
    errno = err;
    return -1;
}

int tcp_fd(int s) {
    struct tcp_listener *lst = hquery(s, tcp_listener_type);
    if(lst) return lst->fd;
    struct tcp_conn *conn = hquery(s, tcp_type);
    if(conn) return conn->fd;
    return -1;
}

static void tcp_listener_hclose(struct hvfs *hvfs) {
    struct tcp_listener *self = (struct tcp_listener*)hvfs;
    fd_close(self->fd);
    free(self);
}

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

static int tcp_makeconn(int fd) {
    int err;
    /* Create the object. */
    struct tcp_conn *self = malloc(sizeof(struct tcp_conn));
    if(dill_slow(!self)) {err = ENOMEM; goto error1;}
    self->hvfs.query = tcp_hquery;
    self->hvfs.close = tcp_hclose;
    self->hvfs.done = tcp_hdone;
    self->bvfs.bsendl = tcp_bsendl;
    self->bvfs.brecvl = tcp_brecvl;
    self->fd = fd;
    fd_initrxbuf(&self->rxbuf);
    self->indone = 0;
    self->outdone = 0;
    self->inerr = 0;
    self->outerr = 0;
    /* Create the handle. */
    int h = hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    free(self);
error1:
    errno = err;
    return -1;
}

