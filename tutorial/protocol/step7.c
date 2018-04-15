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

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "../../libdillimpl.h"

#define cont(ptr, type, member) \
    ((type*)(((char*) ptr) - offsetof(type, member)))

static const int quux_type_placeholder = 0;
static const void *quux_type = &quux_type_placeholder;

struct quux {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    int senderr;
    int recverr;
};

static void *quux_hquery(struct hvfs *hvfs, const void *id);
static void quux_hclose(struct hvfs *hvfs);
static int quux_msendl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t quux_mrecvl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);

int quux_attach(int u) {
    int err;
    struct quux *self = malloc(sizeof(struct quux));
    if(!self) {err = ENOMEM; goto error1;}
    self->hvfs.query = quux_hquery;
    self->hvfs.close = quux_hclose;
    self->mvfs.msendl = quux_msendl;
    self->mvfs.mrecvl = quux_mrecvl;
    self->u = hown(u);
    self->senderr = 0;
    self->recverr = 0;
    int h = hmake(&self->hvfs);
    if(h < 0) {int err = errno; goto error2;}
    return h;
error2:
    free(self);
error1:
    errno = err;
    return -1;
}

int quux_detach(int h) {
    struct quux *self = hquery(h, quux_type);
    if(!self) return -1;
    int u = self->u;
    free(self);
    return u;
}

static void *quux_hquery(struct hvfs *hvfs, const void *type) {
    struct quux *self = (struct quux*)hvfs;
    if(type == msock_type) return &self->mvfs;
    if(type == quux_type) return self;
    errno = ENOTSUP;
    return NULL;
}

static void quux_hclose(struct hvfs *hvfs) {
    struct quux *self = (struct quux*)hvfs;
    free(self);
}

static int quux_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct quux *self = cont(mvfs, struct quux, mvfs);
    if(self->senderr) {errno = ECONNRESET; return -1;}
    size_t sz = 0;
    struct iolist *it;
    for(it = first; it; it = it->iol_next)
        sz += it->iol_len;
    if(sz > 254) {self->senderr = 1; errno = EMSGSIZE; return -1;}
    uint8_t c = (uint8_t)sz;
    struct iolist hdr = {&c, 1, first, 0};
    int rc = bsendl(self->u, &hdr, last, deadline);
    if(rc < 0) {self->senderr = 1; return -1;}
    return 0;
}

static ssize_t quux_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct quux *self = cont(mvfs, struct quux, mvfs);
    if(self->recverr) {errno = ECONNRESET; return -1;}
    uint8_t sz;
    int rc = brecv(self->u, &sz, 1, deadline);
    if(rc < 0) {self->recverr = 1; return -1;}
    if(!first) {
        rc = brecv(self->u, NULL, sz, deadline);
        if(rc < 0) {self->recverr = 1; return -1;}
        return sz;
    }
    size_t bufsz = 0;
    struct iolist *it;
    for(it = first; it; it = it->iol_next)
        bufsz += it->iol_len;
    if(bufsz < sz) {self->recverr = 1; errno = EMSGSIZE; return -1;}
    size_t rmn = sz;
    it = first;
    while(1) {
        if(it->iol_len >= rmn) break;
        rmn -= it->iol_len;
        it = it->iol_next;
        if(!it) {self->recverr = 1; errno = EMSGSIZE; return -1;}
    }
    struct iolist orig = *it;
    it->iol_len = rmn;
    it->iol_next = NULL;
    rc = brecvl(self->u, first, last, deadline);
    *it = orig;
    if(rc < 0) {self->recverr = 1; return -1;}
    return sz;
}

coroutine void client(int s) {
    int q = quux_attach(s);
    assert(q >= 0);
    int rc = msend(q, "Hello, world!", 13, -1);
    assert(rc == 0);
    s = quux_detach(q);
    assert(s >= 0);
    rc = hclose(s);
    assert(rc == 0);
}

int main(void) {
    int ss[2];
    int rc = ipc_pair(ss);
    assert(rc == 0);
    go(client(ss[0]));
    int q = quux_attach(ss[1]);
    assert(q >= 0);
    char buf[256];
    ssize_t sz = mrecv(q, buf, sizeof(buf), -1);
    assert(sz >= 0);
    printf("%.*s\n", (int)sz, buf);
    int s = quux_detach(q);
    assert(s >= 0);
    rc = hclose(s);
    assert(rc == 0);
    return 0;
}

