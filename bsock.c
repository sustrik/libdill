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

dill_unique_id(dill_bsock_type);

int dill_bsend(int s, const void *buf, size_t len, int64_t deadline) {
    int err;
    struct dill_bsock_vfs *b = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!b)) {err = errno; goto error;}
    struct dill_iolist iol = {(void*)buf, len, NULL, 0};
    int rc = b->bsendl(b, &iol, &iol, deadline);
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

int dill_brecv(int s, void *buf, size_t len, int64_t deadline) {
    int err;
    struct dill_bsock_vfs *b = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!b)) {err = errno; goto error;}
    struct dill_iolist iol = {buf, len, NULL, 0};
    int rc = b->brecvl(b, &iol, &iol, deadline);
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

int dill_bsendl(int s, struct dill_iolist *first, struct dill_iolist *last,
      int64_t deadline) {
    int err;
    struct dill_bsock_vfs *b = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!b)) {err = errno; goto error;}
    if(dill_slow(!first || !last || last->iol_next)) {err = EINVAL; goto error;}
    int rc = b->bsendl(b, first, last, deadline);
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

int dill_brecvl(int s, struct dill_iolist *first, struct dill_iolist *last,
      int64_t deadline) {
    int err;
    struct dill_bsock_vfs *b = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!b)) {err = errno; goto error;}
    if(dill_slow((first && !last) || (!first && last) || last->iol_next)) {
        err = EINVAL; goto error;}
    int rc = b->brecvl(b, first, last, deadline);
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

