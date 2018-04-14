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

dill_unique_id(suffix_type);

static void *suffix_hquery(struct hvfs *hvfs, const void *type);
static void suffix_hclose(struct hvfs *hvfs);
static int suffix_msendl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t suffix_mrecvl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

struct suffix_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    /* Given that we are doing one recv call per byte, let's cache the pointer
       to bsock interface of the underlying socket to make it faster. */
    struct bsock_vfs *uvfs;
    uint8_t buf[32];
    uint8_t suffix[32];
    size_t suffixlen;
    unsigned int inerr : 1;
    unsigned int outerr : 1;
    unsigned int mem : 1;
};

//DILL_CT_ASSERT(sizeof(struct suffix_storage) >= sizeof(struct suffix_sock));

static void *suffix_hquery(struct hvfs *hvfs, const void *type) {
    struct suffix_sock *self = (struct suffix_sock*)hvfs;
    if(type == msock_type) return &self->mvfs;
    if(type == suffix_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int dill_suffix_attach_mem(int s, const void *suffix, size_t suffixlen,
      struct suffix_storage *mem) {
    int err;
    if(dill_slow(!mem || !suffix || suffixlen == 0 || suffixlen > 32)) {
        err = EINVAL; goto error1;}
    /* Take ownership of the underlying socket. */
    int u = hown(s);
    if(dill_slow(u < 0)) {err = errno; goto error1;}
    /* Create the object. */
    struct suffix_sock *self = (struct suffix_sock*)mem;
    self->hvfs.query = suffix_hquery;
    self->hvfs.close = suffix_hclose;
    self->mvfs.msendl = suffix_msendl;
    self->mvfs.mrecvl = suffix_mrecvl;
    self->u = u;
    self->uvfs = hquery(u, bsock_type);
    if(dill_slow(!self->uvfs && errno == ENOTSUP)) {err = EPROTO; goto error2;}
    if(dill_slow(!self->uvfs)) {err = errno; goto error2;}
    memcpy(self->suffix, suffix, suffixlen);
    self->suffixlen = suffixlen;
    self->inerr = 0;
    self->outerr = 0;
    self->mem = 1;
    /* Create the handle. */
    int h = hmake(&self->hvfs);
    if(dill_slow(h < 0)) {err = errno; goto error2;}
    return h;
error2:;
    int rc = hclose(u);
    dill_assert(rc == 0);
error1:
    errno = err;
    return -1;
}

int dill_suffix_attach(int s, const void *suffix, size_t suffixlen) {
    int err;
    struct suffix_sock *obj = malloc(sizeof(struct suffix_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int cs = suffix_attach_mem(s, suffix, suffixlen,
        (struct suffix_storage*)obj);
    if(dill_slow(cs < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return cs;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int dill_suffix_detach(int s, int64_t deadline) {
    struct suffix_sock *self = hquery(s, suffix_type);
    if(dill_slow(!self)) return -1;
    int u = self->u;
    if(!self->mem) free(self);
    return u;
}

static int suffix_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct suffix_sock *self = dill_cont(mvfs, struct suffix_sock, mvfs);
    if(dill_slow(self->outerr)) {errno = ECONNRESET; return -1;}
    /* TODO: Make sure that message doesn't contain suffix sequence. */
    struct iolist iol = {self->suffix, self->suffixlen, NULL, 0};
    last->iol_next = &iol;
    int rc = self->uvfs->bsendl(self->uvfs, first, &iol, deadline);
    last->iol_next = NULL;
    if(dill_slow(rc < 0)) {self->outerr = 1; return -1;}
    return 0;
}

static ssize_t suffix_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct suffix_sock *self = dill_cont(mvfs, struct suffix_sock, mvfs);
    if(dill_slow(self->inerr)) {errno = ECONNRESET; return -1;}
    /* First fill in the temporary buffer. */
    struct iolist iol = {self->buf, self->suffixlen, NULL, 0};
    int rc = self->uvfs->brecvl(self->uvfs, &iol, &iol, deadline);
    if(dill_slow(rc < 0)) {self->inerr = 1; return -1;}
    /* Read the input, character by character. */
    iol.iol_base = self->buf + self->suffixlen - 1;
    iol.iol_len = 1;
    size_t sz = 0;
    struct iolist it = *first;
    while(1) {
        /* Check for suffix. */
        if(memcmp(self->buf, self->suffix, self->suffixlen) == 0) break;
        /* Ignore empty iolist items. */
        while(first->iol_len == 0) {
            if(!it.iol_next) {self->inerr = 1; errno = EMSGSIZE; return -1;}
            it = *it.iol_next;
        }
        /* Move one character to the user's iolist. */
        if(it.iol_base) {
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

static void suffix_hclose(struct hvfs *hvfs) {
    struct suffix_sock *self = (struct suffix_sock*)hvfs;
    if(dill_fast(self->u >= 0)) {
        int rc = hclose(self->u);
        dill_assert(rc == 0);
    }
    if(!self->mem) free(self);
}

