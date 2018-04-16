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
    struct dill_msock_vfs *m = dill_hquery(s, dill_msock_type);
    if(dill_slow(!m)) return -1;
    struct dill_iolist iol = {(void*)buf, len, NULL, 0};
    return m->msendl(m, &iol, &iol, deadline);
}

ssize_t dill_mrecv(int s, void *buf, size_t len, int64_t deadline) {
    struct dill_msock_vfs *m = dill_hquery(s, dill_msock_type);
    if(dill_slow(!m)) return -1;
    struct dill_iolist iol = {buf, len, NULL, 0};
    return m->mrecvl(m, &iol, &iol, deadline);
}

int dill_msendl(int s, struct dill_iolist *first, struct dill_iolist *last,
      int64_t deadline) {
    struct dill_msock_vfs *m = dill_hquery(s, dill_msock_type);
    if(dill_slow(!m)) return -1;
    if(dill_slow(!first || !last || last->iol_next)) {
        errno = EINVAL; return -1;}
    return m->msendl(m, first, last, deadline);
}

ssize_t dill_mrecvl(int s, struct dill_iolist *first, struct dill_iolist *last,
      int64_t deadline) {
    struct dill_msock_vfs *m = dill_hquery(s, dill_msock_type);
    if(dill_slow(!m)) return -1;
    if(dill_slow((last && last->iol_next) ||
          (!first && last) ||
          (first && !last))) {
        errno = EINVAL; return -1;}
    return m->mrecvl(m, first, last, deadline);
}

