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
#include <string.h>

#include "utils.h"

dill_unique_id(pfx_type);

static void *pfx_hquery(struct hvfs *hvfs, const void *type);
static void pfx_hclose(struct hvfs *hvfs);
static int pfx_hdone(struct hvfs *hvfs, int64_t deadline);
static int pfx_msendl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t pfx_mrecvl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

struct pfx_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    size_t hdrlen;
    unsigned int bigendian : 1;
    unsigned int indone : 1;
    unsigned int outdone: 1;
    unsigned int inerr : 1;
    unsigned int outerr : 1;
    unsigned int mem : 1;
};

DILL_CT_ASSERT(PFX_SIZE >= sizeof(struct pfx_sock));

static void *pfx_hquery(struct hvfs *hvfs, const void *type) {
    struct pfx_sock *self = (struct pfx_sock*)hvfs;
    if(type == msock_type) return &self->mvfs;
    if(type == pfx_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int pfx_attach_mem(int s, size_t hdrlen, int flags, void *mem) {
    int err;
    if(dill_slow(!mem || hdrlen == 0)) {err = EINVAL; goto error1;}
    /* Make a private copy of the underlying socket. */
    int u = hdup(s);
    if(dill_slow(u < 0)) {err = errno; goto error1;}
    int rc = hclose(s);
    dill_assert(rc == 0);
    /* Check whether underlying socket is a bytestream. */
    void *q = hquery(u, bsock_type);
    if(dill_slow(!q && errno == ENOTSUP)) {err = EPROTO; goto error2;}
    if(dill_slow(!q)) {err = errno; goto error2;}
    /* Create the object. */
    struct pfx_sock *self = (struct pfx_sock*)mem;
    self->hvfs.query = pfx_hquery;
    self->hvfs.close = pfx_hclose;
    self->hvfs.done = pfx_hdone;
    self->mvfs.msendl = pfx_msendl;
    self->mvfs.mrecvl = pfx_mrecvl;
    self->u = u;
    self->hdrlen = hdrlen;
    self->bigendian = !(flags & PFX_LITTLE_ENDIAN);
    self->indone = 0;
    self->outdone = 0;
    self->inerr = 0;
    self->outerr = 0;
    self->mem = 1;
    /* Create the handle. */
    int h = hmake(&self->hvfs);
    if(dill_slow(h < 0)) {int err = errno; goto error2;}
    return h;
error2:
    rc = hclose(u);
    dill_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

int pfx_attach(int s, size_t hdrlen, int flags) {
    int err;
    struct pfx_sock *obj = malloc(PFX_SIZE);
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int ps = pfx_attach_mem(s, hdrlen, flags, obj);
    if(dill_slow(ps < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return ps;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

static int pfx_hdone(struct hvfs *hvfs, int64_t deadline) {
    struct pfx_sock *self = (struct pfx_sock*)hvfs;
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    uint8_t buf[self->hdrlen];
    memset(buf, 0xff, sizeof(buf));
    int rc = bsend(self->u, buf, sizeof(buf), deadline);
    if(dill_slow(rc < 0)) {self->outerr = 1; return -1;}
    self->outdone = 1;
    return 0;
}

int pfx_detach(int s, int64_t deadline) {
    int err;
    struct pfx_sock *self = hquery(s, pfx_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->inerr || self->outerr)) {err = ECONNRESET; goto error;}
    /* If not done already start the terminal handshake. */
    if(!self->outdone) {
        int rc = pfx_hdone(&self->hvfs, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    /* Drain incoming messages until termination message is received. */
    while(1) {
        ssize_t sz = pfx_mrecvl(&self->mvfs, NULL, NULL, deadline);
        if(sz < 0 && errno == EPIPE) break;
        if(dill_slow(sz < 0)) {err = errno; goto error;}
    }
    int u = self->u;
    if(!self->mem) free(self);
    return u;
error:
    pfx_hclose(&self->hvfs);
    errno = err;
    return -1;
}

static int pfx_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct pfx_sock *self = dill_cont(mvfs, struct pfx_sock, mvfs);
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    uint8_t szbuf[self->hdrlen];
    size_t sz = 0;
    struct iolist *it;
    for(it = first; it; it = it->iol_next)
        sz += it->iol_len;
    int i;
    for(i = 0; i != self->hdrlen; ++i) {
        szbuf[self->bigendian ? (self->hdrlen - i - 1) : i] = sz & 0xff;
        sz >>= 8;
    }
    struct iolist hdr = {szbuf, sizeof(szbuf), first, 0};
    int rc = bsendl(self->u, &hdr, last, deadline);
    if(dill_slow(rc < 0)) {self->outerr = 1; return -1;}
    return 0;
}

static ssize_t pfx_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct pfx_sock *self = dill_cont(mvfs, struct pfx_sock, mvfs);
    if(dill_slow(self->indone)) {errno = EPIPE; return -1;}
    if(dill_slow(self->inerr)) {errno = ECONNRESET; return -1;}
    uint8_t szbuf[self->hdrlen];
    int rc = brecv(self->u, szbuf, self->hdrlen, deadline);
    if(dill_slow(rc < 0)) {self->inerr = 1; return -1;}
    uint64_t sz = 0;
    int done = 1;
    int i;
    for(i = 0; i != self->hdrlen; ++i) {
        uint8_t c = szbuf[self->bigendian ? i : (self->hdrlen - i - 1)];
        if(c != 0xff) done = 0;
        sz <<= 8;
        sz |= c;
    }
    /* Peer is terminating. */
    if(dill_slow(done)) {
        self->indone = 1; errno = EPIPE; return -1;}
    /* Skip the message. */
    if(!first) {
        rc = brecv(self->u, NULL, sz, deadline);
        if(dill_slow(rc < 0)) {self->inerr = 1; return -1;}
        return sz;
    }
    /* Trim iolist to reflect the size of the message. */
    size_t rmn = sz;
    struct iolist *it = first;
    while(1) {
        if(it->iol_len >= rmn) break;
        rmn -= it->iol_len;
        it = it->iol_next;
        if(dill_slow(!it)) {self->inerr = 1; errno = EMSGSIZE; return -1;}
    }
    size_t old_len = it->iol_len;
    struct iolist *old_next = it->iol_next;
    it->iol_len = rmn;
    it->iol_next = NULL;
    rc = brecvl(self->u, first, last, deadline);
    /* Get iolist to its original state. */
    it->iol_len = old_len;
    it->iol_next = old_next;
    if(dill_slow(rc < 0)) {self->inerr = 1; return -1;}
    return sz;
}

static void pfx_hclose(struct hvfs *hvfs) {
    struct pfx_sock *self = (struct pfx_sock*)hvfs;
    if(dill_fast(self->u >= 0)) {
        int rc = hclose(self->u);
        dill_assert(rc == 0);
    }
    if(!self->mem) free(self);
}

