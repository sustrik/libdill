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
#include "tcp.h"

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
static int tcp_brecvl(struct bsock_vfs *bvfs,
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
    struct tcp_conn *obj = (struct tcp_conn*)hvfs;
    if(type == bsock_type) return &obj->bvfs;
    if(type == tcp_type) return obj;
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
    rc = fd_close(s);
    dill_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static int tcp_bsendl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct tcp_conn *obj = dill_cont(bvfs, struct tcp_conn, bvfs);
    if(dill_slow(obj->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(obj->outerr)) {errno = ECONNRESET; return -1;}
    ssize_t sz = fd_send(obj->fd, first, last, deadline);
    if(dill_fast(sz >= 0)) return sz;
    obj->outerr = 1;
    return -1;
}

static int tcp_brecvl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct tcp_conn *obj = dill_cont(bvfs, struct tcp_conn, bvfs);
    if(dill_slow(obj->indone)) {errno = EPIPE; return -1;}
    if(dill_slow(obj->inerr)) {errno = ECONNRESET; return -1;}
    int rc = fd_recv(obj->fd, &obj->rxbuf, first, last, deadline);
    if(dill_fast(rc == 0)) return 0;
    if(errno == EPIPE) obj->indone = 1;
    else obj->inerr = 1;
    return -1;
}

static int tcp_hdone(struct hvfs *hvfs, int64_t deadline) {
    struct tcp_conn *obj = (struct tcp_conn*)hvfs;
    if(dill_slow(obj->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(obj->outerr)) {errno = ECONNRESET; return -1;}
    /* Flushing the tx buffer is done asynchronously on kernel level. */
    int rc = shutdown(obj->fd, SHUT_WR);
    dill_assert(rc == 0);
    obj->outdone = 1;
    return 0;
}

int tcp_close(int s, int64_t deadline) {
    int err;
    struct tcp_conn *obj = hquery(s, tcp_type);
    if(dill_slow(!obj)) return -1;
    if(dill_slow(obj->inerr || obj->outerr)) {err = ECONNRESET; goto error;}
    /* If not done already, flush the outbound data and start the terminal
       handshake. */
    if(!obj->outdone) {
        int rc = tcp_hdone(&obj->hvfs, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Now we are going to read all the inbound data until we reach end of the
       stream. That way we can be sure that the peer either received all our
       data or consciously closed the connection without reading all of it. */
    while(1) {
        char buf[128];
        struct iolist iol = {buf, sizeof(buf), NULL, 0};
        int rc = tcp_brecvl(&obj->bvfs, &iol, &iol, deadline);
        if(rc < 0 && errno == EPIPE) break;
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    return 0;
error:
    tcp_hclose(&obj->hvfs);
    errno = err;
    return -1;
}

static void tcp_hclose(struct hvfs *hvfs) {
    struct tcp_conn *obj = (struct tcp_conn*)hvfs;
    int rc = fd_close(obj->fd);
    dill_assert(rc == 0);
    free(obj);
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
    struct tcp_listener *obj = (struct tcp_listener*)hvfs;
    if(type == tcp_listener_type) return obj;
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
    struct tcp_listener *obj = malloc(sizeof(struct tcp_listener));
    if(dill_slow(!obj)) {err = ENOMEM; goto error2;}
    obj->hvfs.query = tcp_listener_hquery;
    obj->hvfs.close = tcp_listener_hclose;
    obj->hvfs.done = NULL;
    obj->fd = s;
    /* Create handle. */
    int h = hmake(&obj->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error3;}
    return h;
error3:
    free(obj);
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
    rc = fd_close(as);
    dill_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

DILL_EXPORT int tcp_fd(int s) {
    struct tcp_listener *lst = hquery(s, tcp_listener_type);
    if(lst) return lst->fd;
    struct tcp_conn *conn = hquery(s, tcp_type);
    if(conn) return conn->fd;
    return -1;
}

static void tcp_listener_hclose(struct hvfs *hvfs) {
    struct tcp_listener *obj = (struct tcp_listener*)hvfs;
    int rc = fd_close(obj->fd);
    dill_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

static int tcp_makeconn(int fd) {
    int err;
    /* Create the object. */
    struct tcp_conn *obj = malloc(sizeof(struct tcp_conn));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    obj->hvfs.query = tcp_hquery;
    obj->hvfs.close = tcp_hclose;
    obj->hvfs.done = tcp_hdone;
    obj->bvfs.bsendl = tcp_bsendl;
    obj->bvfs.brecvl = tcp_brecvl;
    obj->fd = fd;
    fd_initrxbuf(&obj->rxbuf);
    obj->indone = 0;
    obj->outdone = 0;
    obj->inerr = 0;
    obj->outerr = 0;
    /* Create the handle. */
    int h = hmake(&obj->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

