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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "utils.h"

dill_unique_id(dill_suffix_type);

static void *dill_suffix_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_suffix_hclose(struct dill_hvfs *hvfs);
static int dill_suffix_msendl(struct dill_msock_vfs *mvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
static ssize_t dill_suffix_mrecvl(struct dill_msock_vfs *mvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);

struct dill_suffix_sock {
    struct dill_hvfs hvfs;
    struct dill_msock_vfs mvfs;
    int u;
    /* Given that we are doing one recv call per byte, let's cache the pointer
       to bsock interface of the underlying socket to make it faster. */
    struct dill_bsock_vfs *uvfs;
    uint8_t buf[32];
    uint8_t suffix[32];
    size_t suffixlen;
    unsigned int inerr : 1;
    unsigned int outerr : 1;
    unsigned int mem : 1;
};

DILL_CHECK_STORAGE(dill_suffix_sock, dill_suffix_storage)

static void *dill_suffix_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_suffix_sock *self = (struct dill_suffix_sock*)hvfs;
    if(type == dill_msock_type) return &self->mvfs;
    if(type == dill_suffix_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int dill_suffix_attach_mem(int s, const void *suffix, size_t suffixlen,
      struct dill_suffix_storage *mem) {
    int err;
    if(dill_slow(!mem || !suffix || suffixlen == 0 || suffixlen > 32)) {
        err = EINVAL; goto error;}
    /* Take ownership of the underlying socket. */
    s = dill_hown(s);
    if(dill_slow(s < 0)) {err = errno; goto error;}
    /* Create the object. */
    struct dill_suffix_sock *self = (struct dill_suffix_sock*)mem;
    self->hvfs.query = dill_suffix_hquery;
    self->hvfs.close = dill_suffix_hclose;
    self->mvfs.msendl = dill_suffix_msendl;
    self->mvfs.mrecvl = dill_suffix_mrecvl;
    self->u = s;
    self->uvfs = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!self->uvfs && errno == ENOTSUP)) {err = EPROTO; goto error;}
    if(dill_slow(!self->uvfs)) {err = errno; goto error;}
    memcpy(self->suffix, suffix, suffixlen);
    self->suffixlen = suffixlen;
    self->inerr = 0;
    self->outerr = 0;
    self->mem = 1;
    /* Create the handle. */
    int h = dill_hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error;}
    return h;
error:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_suffix_attach(int s, const void *suffix, size_t suffixlen) {
    int err;
    struct dill_suffix_sock *obj = malloc(sizeof(struct dill_suffix_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    s = dill_suffix_attach_mem(s, suffix, suffixlen,
        (struct dill_suffix_storage*)obj);
    if(dill_slow(s < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return s;
error2:
    free(obj);
error1:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_suffix_detach(int s, int64_t deadline) {
    int err;
    struct dill_suffix_sock *self = dill_hquery(s, dill_suffix_type);
    if(dill_slow(!self)) {err = errno; goto error;}
    if(dill_slow(self->inerr || self->outerr)) {err = ECONNRESET; goto error;}
    int u = self->u;
    if(!self->mem) free(self);
    return u;
error:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

static int dill_suffix_msendl(struct dill_msock_vfs *mvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_suffix_sock *self = dill_cont(mvfs, struct dill_suffix_sock,
        mvfs);
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    /* TODO: Make sure that message doesn't contain suffix sequence. */
    struct dill_iolist iol = {self->suffix, self->suffixlen, NULL, 0};
    last->iol_next = &iol;
    int rc = self->uvfs->bsendl(self->uvfs, first, &iol, deadline);
    last->iol_next = NULL;
    if(dill_slow(rc < 0)) {self->outerr = 1; return -1;}
    return 0;
}

static ssize_t dill_suffix_mrecvl(struct dill_msock_vfs *mvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_suffix_sock *self = dill_cont(mvfs, struct dill_suffix_sock,
        mvfs);
    if(dill_slow(self->inerr)) {errno = ECONNRESET; return -1;}
    /* First fill in the temporary buffer. */
    struct dill_iolist iol = {self->buf, self->suffixlen, NULL, 0};
    int rc = self->uvfs->brecvl(self->uvfs, &iol, &iol, deadline);
    if(dill_slow(rc < 0)) {self->inerr = 1; return -1;}
    /* Read the input, character by character. */
    iol.iol_base = self->buf + self->suffixlen - 1;
    iol.iol_len = 1;
    size_t sz = 0;
    struct dill_iolist it = *first;
    while(1) {
        /* Check for suffix. */
        if(memcmp(self->buf, self->suffix, self->suffixlen) == 0) break;
        /* Ignore empty iolist items. */
        while(first->iol_len == 0) {
            if(!it.iol_next) {self->inerr = 1; errno = EMSGSIZE; return -1;}
            it = *it.iol_next;
        }
        /* Move one character to the user's iolist. */
        if(it.iol_base && it.iol_len > 0) {
            ((uint8_t*)it.iol_base)[0] = self->buf[0];
            it.iol_base = ((uint8_t*)it.iol_base) + 1;
            it.iol_len--;
        }
        /* Read new character into the temporary buffer. */
        memmove(self->buf, self->buf + 1, self->suffixlen - 1);
        rc = self->uvfs->brecvl(self->uvfs, &iol, &iol, deadline);
        if(dill_slow(rc < 0)) {self->inerr = 1; return -1;}
        sz++;
    }
    return sz;
}

static void dill_suffix_hclose(struct dill_hvfs *hvfs) {
    struct dill_suffix_sock *self = (struct dill_suffix_sock*)hvfs;
    if(dill_fast(self->u >= 0)) {
        int rc = dill_hclose(self->u);
        dill_assert(rc == 0);
    }
    if(!self->mem) free(self);
}

