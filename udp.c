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
#include <unistd.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "fd.h"
#include "iol.h"
#include "utils.h"

dill_unique_id(dill_udp_type);

static void *dill_udp_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_udp_hclose(struct dill_hvfs *hvfs);
static int dill_udp_msendl(struct dill_msock_vfs *mvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
static ssize_t dill_udp_mrecvl(struct dill_msock_vfs *mvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);

struct dill_udp_sock {
    struct dill_hvfs hvfs;
    struct dill_msock_vfs mvfs;
    int fd;
    struct dill_ipaddr remote;
    unsigned int busy : 1;
    unsigned int hasremote : 1;
    unsigned int mem : 1;
};

DILL_CHECK_STORAGE(dill_udp_sock, dill_udp_storage)

static void *dill_udp_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_udp_sock *obj = (struct dill_udp_sock*)hvfs;
    if(type == dill_msock_type) return &obj->mvfs;
    if(type == dill_udp_type) return obj;
    errno = ENOTSUP;
    return NULL;
}

int dill_udp_open_mem(struct dill_ipaddr *local,
      const struct dill_ipaddr *remote, struct dill_udp_storage *mem) {
    int err;
    /* Sanity checking. */
    if(dill_slow(!mem)) {err = EINVAL; goto error1;}
    if(dill_slow(local && remote &&
          dill_ipaddr_family(local) != dill_ipaddr_family(remote))) {
        err = EINVAL; goto error1;}
    /* Open the listening socket. */
    int family = AF_INET;
    if(local) family = dill_ipaddr_family(local);
    if(remote) family = dill_ipaddr_family(remote);
    int s = socket(family, SOCK_DGRAM, 0);
    if(s < 0) {err = errno; goto error1;}
    /* Set it to non-blocking mode. */
    int rc = dill_fd_unblock(s);
    if(dill_slow(rc < 0)) {err = errno; goto error2;}
    /* Start listening. */
    if(local) {
        rc = bind(s, dill_ipaddr_sockaddr(local), dill_ipaddr_len(local));
        if(s < 0) {err = errno; goto error2;}
        /* Get the ephemeral port number. */
        if(dill_ipaddr_port(local) == 0) {
            struct dill_ipaddr baddr;
            socklen_t len = sizeof(struct dill_ipaddr);
            rc = getsockname(s, (struct sockaddr*)&baddr, &len);
            if(dill_slow(rc < 0)) {err = errno; goto error2;}
            dill_ipaddr_setport(local, dill_ipaddr_port(&baddr));
        }
    }
    /* Create the object. */
    struct dill_udp_sock *obj = (struct dill_udp_sock*)mem;
    obj->hvfs.query = dill_udp_hquery;
    obj->hvfs.close = dill_udp_hclose;
    obj->mvfs.msendl = dill_udp_msendl;
    obj->mvfs.mrecvl = dill_udp_mrecvl;
    obj->fd = s;
    obj->busy = 0;
    obj->hasremote = remote ? 1 : 0;
    obj->mem = 1;
    if(remote) obj->remote = *remote;
    /* Create the handle. */
    int h = dill_hmake(&obj->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:
    dill_fd_close(s);
error1:
    errno = err;
    return -1;
}

int dill_udp_open(struct dill_ipaddr *local, const struct dill_ipaddr *remote) {
    int err;
    struct dill_udp_sock *obj = malloc(sizeof(struct dill_udp_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int s = dill_udp_open_mem(local, remote, (struct dill_udp_storage*)obj);
    if(dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

static int dill_udp_sendl_(struct dill_msock_vfs *mvfs,
      const struct dill_ipaddr *addr,
      struct dill_iolist *first, struct dill_iolist *last) {
    struct dill_udp_sock *obj = dill_cont(mvfs, struct dill_udp_sock, mvfs);
    if(dill_slow(obj->busy)) {errno = EBUSY; return -1;}
    /* If no destination IP address is provided, fall back to the stored one. */
    const struct dill_ipaddr *dstaddr = addr;
    if(!dstaddr) {
        if(dill_slow(!obj->hasremote)) {errno = EINVAL; return -1;}
        dstaddr = &obj->remote;
    }
    struct msghdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.msg_name = (void*)dill_ipaddr_sockaddr(dstaddr);
    hdr.msg_namelen = dill_ipaddr_len(dstaddr);
    /* Make a local iovec array. */
    /* TODO: This is dangerous, it may cause stack overflow.
       There should probably be a on-heap per-socket buffer for that. */
    size_t niov;
    int rc = dill_iolcheck(first, last, &niov, NULL);
    if(dill_slow(rc < 0)) return -1;
    struct iovec iov[niov];
    dill_ioltoiov(first, iov);
    hdr.msg_iov = (struct iovec*)iov;
    hdr.msg_iovlen = niov;
    ssize_t sz = sendmsg(obj->fd, &hdr, 0);
    if(dill_fast(sz >= 0)) return 0;
    if(errno == EAGAIN || errno == EWOULDBLOCK) return 0;
    return -1;
}

static ssize_t dill_udp_recvl_(struct dill_msock_vfs *mvfs,
      struct dill_ipaddr *addr,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_udp_sock *obj = dill_cont(mvfs, struct dill_udp_sock, mvfs);
    if(dill_slow(obj->busy)) {errno = EBUSY; return -1;}
    struct msghdr hdr;
    struct dill_ipaddr raddr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.msg_name = (void*)&raddr;
    hdr.msg_namelen = sizeof(struct dill_ipaddr);
    /* Make a local iovec array. */
    /* TODO: This is dangerous, it may cause stack overflow.
       There should probably be a on-heap per-socket buffer for that. */
    size_t niov;
    int rc = dill_iolcheck(first, last, &niov, NULL);
    if(dill_slow(rc < 0)) return -1;
    struct iovec iov[niov];
    dill_ioltoiov(first, iov);
    hdr.msg_iov = (struct iovec*)iov;
    hdr.msg_iovlen = niov;
    while(1) {
        ssize_t sz = recvmsg(obj->fd, &hdr, 0);
        if(sz >= 0) {
            /* If remote IP address is specified we'll silently drop all
               packets coming from different addresses. */
            if(obj->hasremote && !dill_ipaddr_equal(&raddr, &obj->remote, 0))
                continue;
            if(addr) *addr = raddr;
            return sz;
        }
        if(errno != EAGAIN && errno != EWOULDBLOCK) return -1;
        obj->busy = 1;
        rc = dill_fdin(obj->fd, deadline);
        obj->busy = 0;
        if(dill_slow(rc < 0)) return -1;
    }
}

int dill_udp_send(int s, const struct dill_ipaddr *addr,
      const void *buf, size_t len) {
    struct dill_msock_vfs *m = dill_hquery(s, dill_msock_type);
    if(dill_slow(!m)) return -1;
    struct dill_iolist iol = {(void*)buf, len, NULL, 0};
    return dill_udp_sendl_(m, addr, &iol, &iol);
}

ssize_t dill_udp_recv(int s, struct dill_ipaddr *addr, void *buf, size_t len,
      int64_t deadline) {
    struct dill_msock_vfs *m = dill_hquery(s, dill_msock_type);
    if(dill_slow(!m)) return -1;
    struct dill_iolist iol = {(void*)buf, len, NULL, 0};
    return dill_udp_recvl_(m, addr, &iol, &iol, deadline);
}

int dill_udp_sendl(int s, const struct dill_ipaddr *addr,
      struct dill_iolist *first, struct dill_iolist *last) {
    struct dill_msock_vfs *m = dill_hquery(s, dill_msock_type);
    if(dill_slow(!m)) return -1;
    return dill_udp_sendl_(m, addr, first, last);
}

ssize_t dill_udp_recvl(int s, struct dill_ipaddr *addr,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_msock_vfs *m = dill_hquery(s, dill_msock_type);
    if(dill_slow(!m)) return -1;
    return dill_udp_recvl_(m, addr, first, last, deadline);
}

static int dill_udp_msendl(struct dill_msock_vfs *mvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    return dill_udp_sendl_(mvfs, NULL, first, last);
}

static ssize_t dill_udp_mrecvl(struct dill_msock_vfs *mvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    return dill_udp_recvl_(mvfs, NULL, first, last, deadline);
}

static void dill_udp_hclose(struct dill_hvfs *hvfs) {
    struct dill_udp_sock *obj = (struct dill_udp_sock*)hvfs;
    /* We do not switch off linger here because if UDP socket was fully
       implemented in user space, msend() would block until the packet
       was flushed into network, thus providing some basic reliability.
       Kernel-space implementation here, on the other hand, may queue
       outgoing packets rather than flushing them. The effect is balanced
       out by lingering when closing the socket. */
    dill_fd_close(obj->fd);
    if(!obj->mem) free(obj);
}

