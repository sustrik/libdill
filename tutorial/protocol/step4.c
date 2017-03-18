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
#include <stdlib.h>

#include "../../libdillimpl.h"

static const int quux_type_placeholder = 0;
static const void *quux_type = &quux_type_placeholder;

struct quux {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
};

static void *quux_hquery(struct hvfs *hvfs, const void *type);
static void quux_hclose(struct hvfs *hvfs);
static int quux_hdone(struct hvfs *hvfs, int64_t deadline);
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
    self->hvfs.done = quux_hdone;
    self->mvfs.msendl = quux_msendl;
    self->mvfs.mrecvl = quux_mrecvl;
    self->u = u;
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

static int quux_hdone(struct hvfs *hvfs, int64_t deadline) {
    errno = ENOTSUP;
    return -1;
}

static int quux_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    errno = ENOTSUP;
    return -1;
}

static ssize_t quux_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    errno = ENOTSUP;
    return -1;
}

int main(void) {
    int ss[2];
    int rc = ipc_pair(ss);
    assert(rc == 0);
    int q1 = quux_attach(ss[0]);
    assert(q1 >= 0);
    int q2 = quux_attach(ss[1]);
    assert(q2 >= 0);
    /* Do something useful here! */
    int s = quux_detach(q2);
    assert(s >= 0);
    rc = hclose(s);
    assert(rc == 0);
    s = quux_detach(q1);
    assert(s >= 0);
    rc = hclose(s);
    assert(rc == 0);
    return 0;
}

