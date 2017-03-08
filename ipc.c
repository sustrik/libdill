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
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include "libdillimpl.h"
#include "fd.h"
#include "utils.h"

static int ipc_resolve(const char *addr, struct sockaddr_un *su);
static int ipc_makeconn(int fd);

/******************************************************************************/
/*  UNIX connection socket                                                    */
/******************************************************************************/

dill_unique_id(ipc_type);

static void *ipc_hquery(struct hvfs *hvfs, const void *type);
static void ipc_hclose(struct hvfs *hvfs);
static int ipc_hdone(struct hvfs *hvfs);
static int ipc_bsendl(struct bsock_vfs *bvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static int ipc_brecvl(struct bsock_vfs *bvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

struct ipc_conn {
    struct hvfs hvfs;
    struct bsock_vfs bvfs;
    int fd;
    struct fd_rxbuf rxbuf;
    unsigned int indone : 1;
    unsigned int outdone : 1;
    unsigned int inerr : 1;
    unsigned int outerr : 1;
};

static void *ipc_hquery(struct hvfs *hvfs, const void *type) {
    struct ipc_conn *obj = (struct ipc_conn*)hvfs;
    if(type == bsock_type) return &obj->bvfs;
    if(type == ipc_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int ipc_connect(const char *addr, int64_t deadline) {
    int err;
    /* Create a UNIX address out of the address string. */
    struct sockaddr_un su;
    int rc = ipc_resolve(addr, &su);
    if(rc < 0) {err = errno; goto error1;}
    /* Open a socket. */
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    rc = fd_unblock(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Connect to the remote endpoint. */
    rc = fd_connect(s, (struct sockaddr*)&su, sizeof(su), deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = ipc_makeconn(s);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = fd_close(s);
    dill_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static int ipc_bsendl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct ipc_conn *obj = dill_cont(bvfs, struct ipc_conn, bvfs);
    if(dill_slow(obj->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(obj->outerr)) {errno = ECONNRESET; return -1;}
    ssize_t sz = fd_send(obj->fd, first, last, deadline);
    if(dill_fast(sz >= 0)) return sz;
    obj->outerr = 1;
    return -1;
}

static int ipc_brecvl(struct bsock_vfs *bvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct ipc_conn *obj = dill_cont(bvfs, struct ipc_conn, bvfs);
    if(dill_slow(obj->indone)) {errno = EPIPE; return -1;}
    if(dill_slow(obj->inerr)) {errno = ECONNRESET; return -1;}
    int rc = fd_recv(obj->fd, &obj->rxbuf, first, last, deadline);
    if(dill_fast(rc == 0)) return 0;
    if(errno == EPIPE) obj->indone = 1;
    else obj->inerr = 1;
    return -1;
}

static int ipc_hdone(struct hvfs *hvfs) {
    struct ipc_conn *obj = (struct ipc_conn*)hvfs;
    if(dill_slow(obj->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(obj->outerr)) {errno = ECONNRESET; return -1;}
    /* Flushing the tx buffer is done asynchronously on kernel level. */
    int rc = shutdown(obj->fd, SHUT_WR);
    dill_assert(rc == 0);
    obj->outdone = 1;
    return 0;
}

int ipc_close(int s, int64_t deadline) {
    int err;
    struct ipc_conn *obj = hquery(s, ipc_type);
    if(dill_slow(!obj)) return -1;
    if(dill_slow(obj->inerr || obj->outerr)) {err = ECONNRESET; goto error;}
    /* If not done already, flush the outbound data and start the terminal
       handshake. */
    if(!obj->outdone) {
        int rc = ipc_hdone(&obj->hvfs);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Now we are going to read all the inbound data until we reach end of the
       stream. That way we can be sure that the peer either received all our
       data or consciously closed the connection without reading all of it. */
    while(1) {
        char buf[128];
        struct iolist iol = {buf, sizeof(buf), NULL};
        int rc = ipc_brecvl(&obj->bvfs, &iol, &iol, deadline);
        if(rc < 0 && errno == EPIPE) break;
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    return 0;
error:
    ipc_hclose(&obj->hvfs);
    errno = err;
    return -1;
}

static void ipc_hclose(struct hvfs *hvfs) {
    struct ipc_conn *obj = (struct ipc_conn*)hvfs;
    int rc = fd_close(obj->fd);
    dill_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  UNIX listener socket                                                      */
/******************************************************************************/

dill_unique_id(ipc_listener_type);

static void *ipc_listener_hquery(struct hvfs *hvfs, const void *type);
static void ipc_listener_hclose(struct hvfs *hvfs);

struct ipc_listener {
    struct hvfs hvfs;
    int fd;
};

static void *ipc_listener_hquery(struct hvfs *hvfs, const void *type) {
    struct ipc_listener *obj = (struct ipc_listener*)hvfs;
    if(type == ipc_listener_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int ipc_listen(const char *addr, int backlog) {
    int err;
    /* Create a UNIX address out of the address string. */
    struct sockaddr_un su;
    int rc = ipc_resolve(addr, &su);
    if(rc < 0) {err = errno; goto error1;}
    /* Open the listening socket. */
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if(dill_slow(s < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    rc = fd_unblock(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Start listening for incoming connections. */
    rc = bind(s, (struct sockaddr*)&su, sizeof(su));
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    rc = listen(s, backlog);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the object. */
    struct ipc_listener *obj = malloc(sizeof(struct ipc_listener));
    if(dill_slow(!obj)) {err = ENOMEM; goto error2;}
    obj->hvfs.query = ipc_listener_hquery;
    obj->hvfs.close = ipc_listener_hclose;
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

int ipc_accept(int s, int64_t deadline) {
    int err;
    /* Retrieve the listener object. */
    struct ipc_listener *lst = hquery(s, ipc_listener_type);
    if(dill_slow(!lst)) {err = errno; goto error1;}
    /* Try to get new connection in a non-blocking way. */
    int as = fd_accept(lst->fd, NULL, NULL, deadline);
    if(dill_slow(as < 0)) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = fd_unblock(as);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Create the handle. */
    int h = ipc_makeconn(as);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    rc = fd_close(as);
    dill_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

static void ipc_listener_hclose(struct hvfs *hvfs) {
    struct ipc_listener *obj = (struct ipc_listener*)hvfs;
    int rc = fd_close(obj->fd);
    dill_assert(rc == 0);
    free(obj);
}

/******************************************************************************/
/*  UNIX pair                                                                 */
/******************************************************************************/

int ipc_pair(int s[2]) {
    int err;
    /* Create the pair. */
    int fds[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    if(rc < 0) {err = errno; goto error1;}
    /* Set the sockets to non-blocking mode. */
    rc = fd_unblock(fds[0]);
    if(dill_slow(rc < 0)) {err = errno; goto error3;}
    rc = fd_unblock(fds[1]);
    if(dill_slow(rc < 0)) {err = errno; goto error3;}
    /* Create the handles. */
    s[0] = ipc_makeconn(fds[0]);
    if(dill_slow(s[0] < 0)) {err = errno; goto error3;}
    s[1] = ipc_makeconn(fds[1]);
    if(dill_slow(s[1] < 0)) {err = errno; goto error4;}
    return 0;
error4:
    rc = hclose(s[0]);
    goto error2;
error3:
    rc = fd_close(fds[0]);
    dill_assert(rc == 0);
error2:
    rc = fd_close(fds[1]);
    dill_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

static int ipc_resolve(const char *addr, struct sockaddr_un *su) {
    dill_assert(su);
    if(strlen(addr) >= sizeof(su->sun_path)) {errno = ENAMETOOLONG; return -1;}
    su->sun_family = AF_UNIX;
    strncpy(su->sun_path, addr, sizeof(su->sun_path));
    return 0;
}

static int ipc_makeconn(int fd) {
    int err;
    /* Create the object. */
    struct ipc_conn *obj = malloc(sizeof(struct ipc_conn));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    obj->hvfs.query = ipc_hquery;
    obj->hvfs.close = ipc_hclose;
    obj->hvfs.done = ipc_hdone;
    obj->bvfs.bsendl = ipc_bsendl;
    obj->bvfs.brecvl = ipc_brecvl;
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

