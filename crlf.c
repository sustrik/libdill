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
#include <libdillimpl.h>
#include <stdint.h>
#include <stdlib.h>

#include "utils.h"

dill_unique_id(crlf_type);

static void *crlf_hquery(struct hvfs *hvfs, const void *type);
static void crlf_hclose(struct hvfs *hvfs);
static int crlf_hdone(struct hvfs *hvfs, int64_t deadline);
static int crlf_msendl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t crlf_mrecvl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

struct crlf_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    /* Given that we are doing one recv call per byte, let's cache the pointer
       to bsock interface of the underlying socket to make it faster. */
    struct bsock_vfs *uvfs;
    unsigned int indone : 1;
    unsigned int outdone : 1;
    unsigned int inerr : 1;
    unsigned int outerr : 1;
};

static void *crlf_hquery(struct hvfs *hvfs, const void *type) {
    struct crlf_sock *self = (struct crlf_sock*)hvfs;
    if(type == msock_type) return &self->mvfs;
    if(type == crlf_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int crlf_attach(int s) {
    int err;
    /* Make a private copy of the underlying socket. */
    int u = hdup(s);
    if(dill_slow(u < 0)) return -1;
    int rc = hclose(s);
    dill_assert(rc == 0);
    /* Create the object. */
    struct crlf_sock *self = malloc(sizeof(struct crlf_sock));
    if(dill_slow(!self)) {err = ENOMEM; goto error1;}
    self->hvfs.query = crlf_hquery;
    self->hvfs.close = crlf_hclose;
    self->hvfs.done = crlf_hdone;
    self->mvfs.msendl = crlf_msendl;
    self->mvfs.mrecvl = crlf_mrecvl;
    self->u = u;
    self->uvfs = hquery(u, bsock_type);
    if(dill_slow(!self->uvfs && errno == ENOTSUP)) {err = EPROTO; goto error2;}
    if(dill_slow(!self->uvfs)) {err = errno; goto error2;}
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
    rc = hclose(u);
    dill_assert(rc == 0);
    errno = err;
    return -1;
}

static int crlf_hdone(struct hvfs *hvfs, int64_t deadline) {
    struct crlf_sock *self = (struct crlf_sock*)hvfs;
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    int rc = bsend(self->u, "\r\n", 2, deadline);
    if(dill_slow(rc < 0)) {self->outerr = 1; return -1;}
    self->outdone = 1;
    return 0;
}

int crlf_detach(int s, int64_t deadline) {
    int err;
    struct crlf_sock *self = hquery(s, crlf_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->inerr || self->outerr)) {err = ECONNRESET; goto error;}
    /* If not done already start the terminal handshake. */
    if(!self->outdone) {
        int rc = crlf_hdone(&self->hvfs, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Drain incoming messages until termination message is received. */
    while(1) {
        ssize_t sz = crlf_mrecvl(&self->mvfs, NULL, NULL, deadline);
        if(sz < 0 && errno == EPIPE) break;
        if(dill_slow(sz < 0)) {err = errno; goto error;}
    }
    int u = self->u;
    free(self);
    return u;
error:
    crlf_hclose(&self->hvfs);
    errno = err;
    return -1;
}

static int crlf_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct crlf_sock *self = dill_cont(mvfs, struct crlf_sock, mvfs);
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    /* Make sure that message doesn't contain CRLF sequence. */
    uint8_t c = 0;
    size_t sz = 0;
    struct iolist *it;
    for(it = first; it; it = it->iol_next) {
        int i;
        for(i = 0; i != it->iol_len; ++i) {
            uint8_t c2 = ((uint8_t*)it->iol_base)[i];
            if(dill_slow(c == '\r' && c2 == '\n')) {
                self->outerr = 1; errno = EINVAL; return -1;}
            c = c2;
        }
        sz += it->iol_len;
    }
    /* Can't send empty line. Empty line is used as protocol terminator. */
    if(dill_slow(sz == 0)) {self->outerr = 1; errno = EINVAL; return -1;}
    struct iolist iol = {(void*)"\r\n", 2, NULL, 0};
    last->iol_next = &iol;
    int rc = self->uvfs->bsendl(self->uvfs, first, &iol, deadline);
    last->iol_next = NULL;
    if(dill_slow(rc < 0)) {self->outerr = 1; return -1;}
    return 0;
}

static ssize_t crlf_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct crlf_sock *self = dill_cont(mvfs, struct crlf_sock, mvfs);
    if(dill_slow(self->indone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->inerr)) {errno = ECONNRESET; return -1;}
    size_t recvd = 0;
    char c1 = 0;
    char c2 = 0;
    struct iolist iol = {&c2, 1, NULL, 0};
    struct iolist *it = first;
    size_t column = 0;
    while(1) {
        /* The pipeline looks like this: buffer <- c1 <- c2 <- socket */
        /* buffer <- c1 */
        if(first) {
            if(!it) {self->inerr = 1; errno = EMSGSIZE; return -1;}
            if(recvd > 1) {
                if(it->iol_base) ((char*)it->iol_base)[column] = c1;
                ++column;
                if(column == it->iol_len) {
                    column = 0;
                    it = it->iol_next;
                }
            }
        }
        /* c1 <- c2 */
        c1 = c2;
        /* c2 <- socket */
        int rc = self->uvfs->brecvl(self->uvfs, &iol, &iol, deadline);
        if(dill_slow(rc < 0)) {self->inerr = 1; return -1;}
        ++recvd;
        if(c1 == '\r' && c2 == '\n') break;
    }
    /* Empty line means that peer is terminating. */
    if(dill_slow(recvd == 2)) {self->indone = 1; errno = EPIPE; return -1;}
    return recvd - 2;
}

static void crlf_hclose(struct hvfs *hvfs) {
    struct crlf_sock *self = (struct crlf_sock*)hvfs;
    if(dill_fast(self->u >= 0)) {
        int rc = hclose(self->u);
        dill_assert(rc == 0);
    }
    free(self);
}

