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
#include <stddef.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdillimpl.h"
#include "utils.h"

dill_unique_id(dill_msock_type);

int dill_msend(int s, const void *buf, size_t len, int64_t deadline) {
    int err;
    struct dill_msock_vfs *self = dill_hquery(s, dill_msock_type);
    if(dill_slow(!self)) {err = errno; goto error;}
    struct dill_iolist iol = {(void*)buf, len, NULL, 0};
    int rc = self->msendl(self, &iol, &iol, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    return 0;
error:
    if(err != EBADF && err != EBUSY && err != EPIPE) {
        rc = dill_hnullify(s);
        dill_assert(rc == 0);
    }
    errno = err;
    return -1;
}

ssize_t dill_mrecv(int s, void *buf, size_t len, int64_t deadline) {
    int err;
    struct dill_msock_vfs *self = dill_hquery(s, dill_msock_type);
    if(dill_slow(!self)) {err = errno; goto error;}
    struct dill_iolist iol = {buf, len, NULL, 0};
    ssize_t sz = self->mrecvl(self, &iol, &iol, deadline);
    if(dill_slow(sz < 0)) {err = errno; goto error;}
    return sz;
error:
    if(err != EBADF && err != EBUSY && err != EPIPE) {
        int rc = dill_hnullify(s);
        dill_assert(rc == 0);
    }
    errno = err;
    return -1;
}

int dill_msendl(int s, struct dill_iolist *first, struct dill_iolist *last,
      int64_t deadline) {
    int err;
    struct dill_msock_vfs *self = dill_hquery(s, dill_msock_type);
    if(dill_slow(!self)) {err = errno; goto error;}
    if(dill_slow(!first || !last || last->iol_next)) {
        err = EINVAL; goto error;}
    int rc = self->msendl(self, first, last, deadline);
    if(dill_slow(rc < 0)) {err = errno; goto error;}
    return 0;
error:
    if(err != EBADF && err != EBUSY && err != EPIPE) {
        rc = dill_hnullify(s);
        dill_assert(rc == 0);
    }
    errno = err;
    return -1;
}

ssize_t dill_mrecvl(int s, struct dill_iolist *first, struct dill_iolist *last,
      int64_t deadline) {
    int err;
    struct dill_msock_vfs *self = dill_hquery(s, dill_msock_type);
    if(dill_slow(!self)) {err = errno; goto error;}
    if(dill_slow((last && last->iol_next) ||
          (!first && last) ||
          (first && !last))) {
        err = EINVAL; goto error;}
    ssize_t sz = self->mrecvl(self, first, last, deadline);
    if(dill_slow(sz < 0)) {err = errno; goto error;}
    return sz; 
error:
    if(err != EBADF && err != EBUSY && err != EPIPE) {
        int rc = dill_hnullify(s);
        dill_assert(rc == 0);
    }
    errno = err;
    return -1;
}

