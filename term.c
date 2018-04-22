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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DILL_DISABLE_RAW_NAMES
#include <libdillimpl.h>
#include "iol.h"
#include "utils.h"

#define DILL_MAX_TERMINATOR_LENGTH 32

dill_unique_id(dill_term_type);

static void *dill_term_hquery(struct dill_hvfs *hvfs, const void *type);
static void dill_term_hclose(struct dill_hvfs *hvfs);
static int dill_term_msendl(struct dill_msock_vfs *mvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);
static ssize_t dill_term_mrecvl(struct dill_msock_vfs *mvfs,
    struct dill_iolist *first, struct dill_iolist *last, int64_t deadline);

struct dill_term_sock {
    struct dill_hvfs hvfs;
    struct dill_msock_vfs mvfs;
    int u;
    size_t len;
    uint8_t buf[DILL_MAX_TERMINATOR_LENGTH];
    unsigned int indone : 1;
    unsigned int outdone : 1;
    unsigned int mem : 1;
};

DILL_CHECK_STORAGE(dill_term_sock, dill_term_storage)

static void *dill_term_hquery(struct dill_hvfs *hvfs, const void *type) {
    struct dill_term_sock *self = (struct dill_term_sock*)hvfs;
    if(type == dill_msock_type) return &self->mvfs;
    if(type == dill_term_type) return self;
    errno = ENOTSUP;
    return NULL;
}

int dill_term_attach_mem(int s, const void *buf, size_t len,
      struct dill_term_storage *mem) {
    int err;
    if(dill_slow(!mem && len > DILL_MAX_TERMINATOR_LENGTH)) {
        err = EINVAL; goto error;}
    if(dill_slow(len > 0 && !buf)) {err = EINVAL; goto error;}
    /* Take ownership of the underlying socket. */
    s = dill_hown(s);
    if(dill_slow(s < 0)) {err = errno; goto error;}
    /* Check whether underlying socket is message-based. */
    void *q = dill_hquery(s, dill_msock_type);
    if(dill_slow(!q && errno == ENOTSUP)) {err = EPROTO; goto error;}
    if(dill_slow(!q)) {err = errno; goto error;}
    /* Create the object. */
    struct dill_term_sock *self = (struct dill_term_sock*)mem;
    self->hvfs.query = dill_term_hquery;
    self->hvfs.close = dill_term_hclose;
    self->mvfs.msendl = dill_term_msendl;
    self->mvfs.mrecvl = dill_term_mrecvl;
    self->u = s;
    self->len = len;
    memcpy(self->buf, buf, len);
    self->indone = 0;
    self->outdone = 0;
    self->mem = 1;
    /* Create the handle. */
    int h = dill_hmake(&self->hvfs);
    if(dill_slow(h < 0)) {int err = errno; goto error;}
    return h;
error:
    if(s >= 0) dill_hclose(s);
    errno = err;
    return -1;
}

int dill_term_attach(int s, const void *buf, size_t len) {
    int err;
    struct dill_term_sock *obj = malloc(sizeof(struct dill_term_sock));
    if(dill_slow(!obj)) {err = ENOMEM; goto error1;}
    s = dill_term_attach_mem(s, buf, len, (struct dill_term_storage*)obj);
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

int dill_term_done(int s, int64_t deadline) {
    struct dill_term_sock *self = dill_hquery(s, dill_term_type);
    if(dill_slow(!self)) return -1;
    if(dill_slow(self->outdone)) {errno = EPIPE; return -1;}
    int rc = dill_msend(self->u, self->buf, self->len, deadline);
    if(dill_slow(rc < 0)) return -1;
    self->outdone = 1;
    return 0;
}

int dill_term_detach(int s, int64_t deadline) {
    int err;
    struct dill_term_sock *self = dill_hquery(s, dill_term_type);
    if(dill_slow(!self)) return -1;
    if(!self->outdone) {
        int rc = dill_term_done(s, deadline);
        if(dill_slow(rc < 0)) {err = errno; goto error;}
    }
    if(!self->indone) {
        while(1) {
            struct dill_iolist iol = {NULL, SIZE_MAX, NULL, 0};
            ssize_t sz = dill_term_mrecvl(&self->mvfs, &iol, &iol, deadline);
            if(sz < 0) {
                if(errno == EPIPE) break;
                err = errno;
                goto error;
            }
        }
    }
    int u = self->u;
    if(!self->mem) free(self);
    return u;
error:;
    int rc = dill_hclose(s);
    dill_assert(rc == 0);
    errno = err;
    return -1;
}

static int dill_term_msendl(struct dill_msock_vfs *mvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_term_sock *self = dill_cont(mvfs, struct dill_term_sock, mvfs);
    /* TODO: Check that it's not the terminal message. */
    return dill_msendl(self->u, first, last, deadline);
}

static ssize_t dill_term_mrecvl(struct dill_msock_vfs *mvfs,
      struct dill_iolist *first, struct dill_iolist *last, int64_t deadline) {
    struct dill_term_sock *self = dill_cont(mvfs, struct dill_term_sock, mvfs);
    if(self->len == 0) {
        ssize_t sz = dill_mrecvl(self->u, first, last, deadline);
        if(dill_slow(sz < 0)) return -1;
        if(dill_slow(sz == 0)) {
            self->indone = 1;
            errno = EPIPE;
            return -1;
        }
        return sz;
    }
    struct dill_iolist trimmed = {0};
    int rc = dill_ioltrim(first, self->len, &trimmed);
    uint8_t buf[self->len];
    struct dill_iolist iol = {buf, self->len, rc < 0 ? NULL : &trimmed, 0}; 
    ssize_t sz = dill_mrecvl(self->u, &iol, rc < 0 ? &iol : last, deadline);
    if(dill_slow(sz < 0)) return -1;
    if(dill_slow(sz == self->len &&
          dill_slow(memcmp(self->buf, buf, self->len) == 0))) {
        self->indone = 1;
        errno = EPIPE;
        return -1;
    }
    dill_iolto(buf, self->len, first);
    return sz;
}

static void dill_term_hclose(struct dill_hvfs *hvfs) {
    struct dill_term_sock *self = (struct dill_term_sock*)hvfs;
    if(dill_fast(self->u >= 0)) {
        int rc = dill_hclose(self->u);
        dill_assert(rc == 0);
    }
    if(!self->mem) free(self);
}

