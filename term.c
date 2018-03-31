/*

  Copyright (c) 2018 Martin Sustrik

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

#include "iol.h"
#include "utils.h"

#define MAX_TERMINATOR_LENGTH 32

dill_unique_id(term_type);

static void *term_hquery(struct hvfs *hvfs, const void *type);
static void term_hclose(struct hvfs *hvfs);
static int term_msendl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t term_mrecvl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

struct term_sock {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    size_t len;
    uint8_t buf[MAX_TERMINATOR_LENGTH];
    unsigned int indone : 1;
    unsigned int outdone : 1;
    unsigned int mem : 1;
};

DILL_CT_ASSERT(sizeof(struct term_storage) >= sizeof(struct term_sock));

static void *term_hquery(struct hvfs *hvfs, const void *type) {
    struct term_sock *self = (struct term_sock*)hvfs;
    if(type == msock_type) return &self->mvfs;
    if(type == term_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int term_attach_mem(int s, const void *buf, size_t len,
      struct term_storage *mem) {
    int err;
    if(dill_slow(!mem && len > MAX_TERMINATOR_LENGTH)) {
        err = EINVAL; goto error1;}
    if(dill_slow(len > 0 && !buf)) {err = EINVAL; goto error1;}
    /* Make a private copy of the underlying socket. */
    int u = hdup(s);
    if(dill_slow(u < 0)) {err = errno; goto error1;}
    int rc = hclose(s);
    dill_assert(rc == 0);
    /* Check whether underlying socket is message-based. */
    void *q = hquery(u, msock_type);
    if(dill_slow(!q && errno == ENOTSUP)) {err = EPROTO; goto error2;}
    if(dill_slow(!q)) {err = errno; goto error2;}
    /* Create the object. */
    struct term_sock *self = (struct term_sock*)mem;
    self->hvfs.query = term_hquery;
    self->hvfs.close = term_hclose;
    self->hvfs.done = NULL;
    self->mvfs.msendl = term_msendl;
    self->mvfs.mrecvl = term_mrecvl;
    self->u = u;
    self->len = len;
    memcpy(self->buf, buf, len);
    self->indone = 0;
    self->outdone = 0;
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

int term_attach(int s, const void *buf, size_t len) {
    int err;
    struct term_sock *obj = malloc(sizeof(struct term_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    int cs = term_attach_mem(s, buf, len, (struct term_storage*)obj);
    if(dill_slow(cs < 0)) {err = errno; goto error2;}
    obj->mem = 0;
    return cs;
error2:
    free(obj);
error1:
    errno = err;
    return -1;
}

int term_done(int s, int64_t deadline) {
    struct term_sock *self = hquery(s, term_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    int rc = msend(self->u, self->buf, self->len, deadline);
    if(dill_slow(rc < 0)) return -1;
    self->outdone = 1;
    return 0;
}

int term_detach(int s, int64_t deadline) {
    int err;
    struct term_sock *self = hquery(s, term_type);
    if(dill_slow(!self)) return -1;
    if(!self->outdone) {
        int rc = term_done(s, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    while(1) {
        struct iolist iol = {NULL, SIZE_MAX, NULL, 0};
        ssize_t sz = term_mrecvl(&self->mvfs, &iol, &iol, deadline);
        if(sz < 0) {
            if(errno == EPIPE) break;
            err = errno;
            goto error;
        }
    }
    int u = self->u;
    if(!self->mem) free(self);
    return u;
error:;
    int rc = hclose(s);
    dill_assert(rc == 0);
    errno = err;
    return -1;
}

static int term_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct term_sock *self = dill_cont(mvfs, struct term_sock, mvfs);
    /* TODO: Check that it's not the terminal message. */
    return msendl(self->u, first, last, deadline);
}

static ssize_t term_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct term_sock *self = dill_cont(mvfs, struct term_sock, mvfs);
    struct iolist trimmed = {0};
    int rc = iol_ltrim(first, self->len, &trimmed);
    uint8_t buf[self->len];
    struct iolist iol = {buf, self->len, rc < 0 ? NULL : &trimmed, 0}; 
    ssize_t sz = mrecvl(self->u, &iol, rc < 0 ? &iol : last, deadline);
    if(dill_slow(sz < 0)) return -1;
    if(dill_slow(sz == self->len &&
          dill_slow(memcmp(self->buf, buf, self->len) == 0))) {
        self->indone = 1;
        errno = EPIPE;
        return -1;
    }
    iol_copy(buf, self->len, first);
    return sz;
}

static void term_hclose(struct hvfs *hvfs) {
    struct term_sock *self = (struct term_sock*)hvfs;
    if(dill_fast(self->u >= 0)) {
        int rc = hclose(self->u);
        dill_assert(rc == 0);
    }
    if(!self->mem) free(self);
}

