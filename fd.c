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

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ctx.h"
#include "fd.h"
#include "iol.h"
#include "utils.h"

#define DILL_FD_CACHESIZE 32
#define DILL_FD_BUFSIZE 1984

#if defined MSG_NOSIGNAL
#define FD_NOSIGNAL MSG_NOSIGNAL
#else
#define FD_NOSIGNAL 0
#endif

int dill_ctx_fd_init(struct dill_ctx_fd *ctx) {
    ctx->count = 0;
    dill_slist_init(&ctx->cache);
    return 0;
}

void dill_ctx_fd_term(struct dill_ctx_fd *ctx) {
    while(1) {
        struct dill_slist *it = dill_slist_pop(&ctx->cache);
        if(it == &ctx->cache) break;
        free(it);
    }
}

static uint8_t *dill_fd_allocbuf(void) {
    struct dill_ctx_fd *ctx = &dill_getctx->fd;
    struct dill_slist *it = dill_slist_pop(&ctx->cache);
    if(dill_fast(it != &ctx->cache)) {
        ctx->count--;
        return (uint8_t*)it;
    }
    uint8_t *p = malloc(DILL_FD_BUFSIZE);
    if(dill_slow(!p)) {errno = ENOMEM; return NULL;}
    return p;
}

static void dill_fd_freebuf(uint8_t *buf) {
    struct dill_ctx_fd *ctx = &dill_getctx->fd;
    if(ctx->count >= DILL_FD_CACHESIZE) {
        free(buf);
        return;
    }
    dill_slist_push(&ctx->cache, (struct dill_slist*)buf);
    ctx->count++;
}

void dill_fd_initrxbuf(struct dill_fd_rxbuf *rxbuf) {
    dill_assert(rxbuf);
    rxbuf->len = 0;
    rxbuf->pos = 0;
    rxbuf->buf = NULL;
}

void dill_fd_termrxbuf(struct dill_fd_rxbuf *rxbuf) {
    if(rxbuf->buf) dill_fd_freebuf(rxbuf->buf);
}

int dill_fd_unblock(int s) {
    /* Switch to non-blocking mode. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    dill_assert(rc == 0);
    /*  Allow re-using the same local address rapidly. */
    opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    dill_assert(rc == 0);
    /* If possible, prevent SIGPIPE signal when writing to the connection
        already closed by the peer. */
#ifdef SO_NOSIGPIPE
    opt = 1;
    rc = setsockopt (s, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof (opt));
    dill_assert (rc == 0 || errno == EINVAL);
#endif
    return 0;
}

int dill_fd_connect(int s, const struct sockaddr *addr, socklen_t addrlen,
      int64_t deadline) {
    /* Initiate connect. */
    int rc = connect(s, addr, addrlen);
    if(rc == 0) return 0;
    if(dill_slow(errno != EINPROGRESS)) return -1;
    /* Connect is in progress. Let's wait till it's done. */
    rc = dill_fdout(s, deadline);
    if(dill_slow(rc == -1)) return -1;
    /* Retrieve the error from the socket, if any. */
    int err = 0;
    socklen_t errsz = sizeof(err);
    rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (void*)&err, &errsz);
    if(dill_slow(rc != 0)) return -1;
    if(dill_slow(err != 0)) {errno = err; return -1;}
    return 0;
}

int dill_fd_accept(int s, struct sockaddr *addr, socklen_t *addrlen,
      int64_t deadline) {
    int as;
    while(1) {
        /* Try to accept new connection synchronously. */
        as = accept(s, addr, addrlen);
        if(dill_fast(as >= 0))
            break;
        /* If connection was aborted by the peer grab the next one. */
        if(dill_slow(errno == ECONNABORTED)) continue;
        /* Propagate other errors to the caller. */
        if(dill_slow(errno != EAGAIN && errno != EWOULDBLOCK)) return -1;
        /* Operation is in progress. Wait till new connection is available. */
        int rc = dill_fdin(s, deadline);
        if(dill_slow(rc < 0)) return -1;
    }
    int rc = dill_fd_unblock(as);
    dill_assert(rc == 0);
    return as;
}

int dill_fd_send(int s, struct dill_iolist *first, struct dill_iolist *last,
      int64_t deadline) {
    /* Make a local iovec array. */
    /* TODO: This is dangerous, it may cause stack overflow.
       There should probably be a on-heap per-socket buffer for that. */
    size_t niov;
    int rc = dill_iolcheck(first, last, &niov, NULL);
    if(dill_slow(rc < 0)) return -1;
    struct iovec iov[niov];
    dill_ioltoiov(first, iov);
    /* Message header will act as an iterator in the following loop. */
    struct msghdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.msg_iov = iov;
    hdr.msg_iovlen = niov;
    /* It is very likely that at least one byte can be sent. Therefore,
       to improve efficiency, try to send and resort to fdout() only after
       send failed. */
    while(1) {
        while(hdr.msg_iovlen && !hdr.msg_iov[0].iov_len) {
            hdr.msg_iov++;
            hdr.msg_iovlen--;
        }
        if(!hdr.msg_iovlen) return 0;
        ssize_t sz = sendmsg(s, &hdr, FD_NOSIGNAL);
        dill_assert(sz != 0);
        if(sz < 0) {
            if(dill_slow(errno != EWOULDBLOCK && errno != EAGAIN)) {
                if(errno == EPIPE) errno = ECONNRESET;
                return -1;
            }
            sz = 0;
        }
        /* Adjust the iovec array so that it doesn't contain data
           that was already sent. */
        while(sz) {
            struct iovec *head = &hdr.msg_iov[0];
            if(head->iov_len > sz) {
                head->iov_base += sz;
                head->iov_len -= sz;
                break;
            }
            sz -= head->iov_len;
            hdr.msg_iov++;
            hdr.msg_iovlen--;
            if(!hdr.msg_iovlen) return 0;
        }
        /* Wait till more data can be sent. */
        int rc = dill_fdout(s, deadline);
        if(dill_slow(rc < 0)) return -1;
    }
}

/* Same as dill_fd_recv() but with no rx buffering. */
static int dill_fd_recv_(int s, struct dill_iolist *first,
      struct dill_iolist *last, int64_t deadline) {
    /* Make a local iovec array. */
    /* TODO: This is dangerous, it may cause stack overflow.
       There should probably be a on-heap per-socket buffer for that. */
    size_t niov;
    int rc = dill_iolcheck(first, last, &niov, NULL);
    if(dill_slow(rc < 0)) return -1;
    struct iovec iov[niov];
    dill_ioltoiov(first, iov);
    /* Message header will act as an iterator in the following loop. */
    struct msghdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.msg_iov = iov;
    hdr.msg_iovlen = niov;
    while(1) {
        ssize_t sz = recvmsg(s, &hdr, 0);
        if(dill_slow(sz == 0)) {errno = EPIPE; return -1;}
        if(sz < 0) {
            if(dill_slow(errno != EWOULDBLOCK && errno != EAGAIN)) {
                if(errno == EPIPE) errno = ECONNRESET;
                return -1;
            }
            sz = 0;
        }
        /* Adjust the iovec array so that it doesn't contain buffers
           that ware already filled in. */
        while(sz) {
            struct iovec *head = &hdr.msg_iov[0];
            if(head->iov_len > sz) {
                head->iov_base += sz;
                head->iov_len -= sz;
                break;
            }
            sz -= head->iov_len;
            hdr.msg_iov++;
            hdr.msg_iovlen--;
            if(!hdr.msg_iovlen) return 0;
        }
        /* Wait for more data. */
        int rc = dill_fdin(s, deadline);
        if(dill_slow(rc < 0)) return -1;
    }
}

/* Skip len bytes. If len is negative skip until error occurs. */
static int dill_fd_skip(int s, ssize_t len, int64_t deadline) {
    uint8_t buf[512];
    while(len) {
        size_t to_recv = len < 0 || len > sizeof(buf) ? sizeof(buf) : len;
        struct dill_iolist iol = {buf, to_recv, NULL, 0};
        int rc = dill_fd_recv_(s, &iol, &iol, deadline);
        if(dill_slow(rc < 0)) return -1;
        if(len >= 0) len -= to_recv;
    }
    return 0;
}

/* Copy data from rxbuf to one dill_iolist structure.
   Returns number of bytes copied. */
static size_t dill_fd_copy(struct dill_fd_rxbuf *rxbuf,
      struct dill_iolist *iol) {
    size_t rmn = rxbuf->len  - rxbuf->pos;
    if(!rmn) return 0;
    if(rmn < iol->iol_len) {
        if(dill_fast(iol->iol_base))
            memcpy(iol->iol_base, rxbuf->buf + rxbuf->pos, rmn);
        rxbuf->len = 0;
        rxbuf->pos = 0;
        dill_fd_freebuf(rxbuf->buf);
        rxbuf->buf = NULL;
        return rmn;
    }
    if(dill_fast(iol->iol_base))
        memcpy(iol->iol_base, rxbuf->buf + rxbuf->pos, iol->iol_len);
    rxbuf->pos += iol->iol_len;
    return iol->iol_len;
}

int dill_fd_recv(int s, struct dill_fd_rxbuf *rxbuf, struct dill_iolist *first,
      struct dill_iolist *last, int64_t deadline) {
    /* Skip all data until error occurs. */
    if(dill_slow(!first && !last)) return dill_fd_skip(s, -1, deadline);
    /* Fill in data from the rxbuf. */
    size_t sz = 0;
    if(dill_fast(rxbuf)) {
        while(1) {
            sz = dill_fd_copy(rxbuf, first);
            if(sz < first->iol_len) break;
            first = first->iol_next;
            if(!first) return 0;
        }
    }
    /* Copy the current iolist element so that we can modify it without
       changing the original list. */
    struct dill_iolist curr;
    curr.iol_base = first->iol_base ? first->iol_base + sz : NULL;
    curr.iol_len = first->iol_len - sz;
    curr.iol_next = first->iol_next;
    curr.iol_rsvd = 0;
    /* Find out how much data is still missing. */
    size_t miss = 0;
    struct dill_iolist *it = &curr;
    while(it) {
        miss += it->iol_len;
        it = it->iol_next;
    }
    /* If requested amount of data is larger than rx buffer avoid the copy
       and read it directly into user's buffer. */
    if(!rxbuf || miss > DILL_FD_BUFSIZE) {
        // There may be NULL bufers in the list. These can't be passed to
        // recv_(). We have to split the list and call recv_() and skip()
        // respectively.
        struct dill_iolist *begin = &curr;
        struct dill_iolist *end = curr.iol_next ? last : &curr;
        struct dill_iolist *it = begin;
        while(1) {
            if(dill_slow(!it->iol_len)) {
                /* No buffer space available. Move on to the next buffer. */
                dill_assert(it == begin);
                goto next;
            }
            if(dill_slow(!it->iol_base)) {
                /* Skip specified number of bytes. */
                dill_assert(it == begin);
                int rc = dill_fd_skip(s, it->iol_len, deadline);
                goto next;
            }
            if(it == end || !it->iol_next->iol_base || !it->iol_next->iol_len) {
                /* Do the actual recv syscall. */
                struct dill_iolist *tmp = it->iol_next;
                it->iol_next = NULL;
                int rc = dill_fd_recv_(s, begin, it, deadline);
                it->iol_next = tmp;
                if(dill_slow(rc < 0)) return -1;
                goto next;
            }
            /* Accumlate multiple buffers into a single syscall. */
            it = it->iol_next;
            continue;
next:
            if(it == end) return 0;
            it = it->iol_next;
            begin = it;
        }
    }
    /* If small amount of data is requested use rx buffer. */
    while(1) {
        /* Read as much data as possible to the buffer to avoid extra
           syscalls. Do the speculative recv() first to avoid extra
           polling. Do fdin() only after recv() fails to get data. */
        if(!rxbuf->buf) {
            rxbuf->buf = dill_fd_allocbuf();
            if(dill_slow(!rxbuf->buf)) return -1;
        }
        ssize_t sz = recv(s, rxbuf->buf, DILL_FD_BUFSIZE, 0);
        if(dill_slow(sz == 0)) {errno = EPIPE; return -1;}
        if(sz < 0) {
            if(dill_slow(errno != EWOULDBLOCK && errno != EAGAIN)) {
                if(errno == EPIPE) errno = ECONNRESET;
                return -1;
            }
            sz = 0;
        }
        rxbuf->len = sz;
        rxbuf->pos = 0;
        /* Copy the data from rxbuffer to the iolist. */
        while(1) {
            sz = dill_fd_copy(rxbuf, &curr);
            if(sz < curr.iol_len) break;
            if(!curr.iol_next) return 0;
            curr = *curr.iol_next;
        }
        if(curr.iol_base) curr.iol_base += sz;
        curr.iol_len -= sz;
        /* Wait for more data. */
        int rc = dill_fdin(s, deadline);
        if(dill_slow(rc < 0)) return -1;
    }
}

void dill_fd_close(int s) {
    int rc = dill_fdclean(s);
    dill_assert(rc == 0);
    /* Discard any pending outbound data. If SO_LINGER option cannot
       be set, never mind and continue anyway. */
    struct linger lng;
    lng.l_onoff=1;
    lng.l_linger=0;
    setsockopt(s, SOL_SOCKET, SO_LINGER, (void*)&lng, sizeof(lng));
    /* We are not checking the error here. close() has inconsistent behaviour
       and leaking a file descriptor is better than crashing the entire
       program. */
    close(s);
}

int dill_fd_own(int s) {
    int n = dup(s);
    if(dill_slow(n < 0)) return -1;
    dill_fd_close(s);
    return n;
}

int dill_fd_check(int s, int type, int family1, int family2, int listening) {
    /* Check type. E.g. SOCK_STREAM vs. SOCK_DGRAM. */
    int val;
    socklen_t valsz = sizeof(val);
    int rc = getsockopt(s, SOL_SOCKET, SO_TYPE, &val, &valsz);
    if(dill_slow(rc < 0)) return -1;
    if(dill_slow(val != type)) {errno = EINVAL; return -1;}
    /* Check whether the socket is in listening mode.
       Returns ENOPROTOOPT on OSX. */
    rc = getsockopt(s, SOL_SOCKET, SO_ACCEPTCONN, &val, &valsz);
    if(dill_slow(rc < 0 && errno != ENOPROTOOPT)) return -1;
    if(dill_slow(rc >= 0 && val != listening)) {errno = EINVAL; return -1;}
    /* Check family. E.g. AF_INET vs. AF_UNIX. */
    struct sockaddr_storage ss;
    socklen_t sssz = sizeof(ss);
    rc = getsockname(s, (struct sockaddr*)&ss, &sssz);
    if(dill_slow(rc < 0)) return -1;
    if(dill_slow(ss.ss_family != family1 && ss.ss_family != family2)) {
        errno = EINVAL; return -1;}
    return 0;
}

