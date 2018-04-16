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
    struct dill_bsock_vfs *b = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!b)) return -1;
    struct dill_iolist iol = {(void*)buf, len, NULL, 0};
    return b->bsendl(b, &iol, &iol, deadline);
}

int dill_brecv(int s, void *buf, size_t len, int64_t deadline) {
    struct dill_bsock_vfs *b = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!b)) return -1;
    struct dill_iolist iol = {buf, len, NULL, 0};
    return b->brecvl(b, &iol, &iol, deadline);
}

int dill_bsendl(int s, struct dill_iolist *first, struct dill_iolist *last,
      int64_t deadline) {
    struct dill_bsock_vfs *b = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!b)) return -1;
    if(dill_slow(!first || !last || last->iol_next)) {
        errno = EINVAL; return -1;}
    return b->bsendl(b, first, last, deadline);
}

int dill_brecvl(int s, struct dill_iolist *first, struct dill_iolist *last,
      int64_t deadline) {
    struct dill_bsock_vfs *b = dill_hquery(s, dill_bsock_type);
    if(dill_slow(!b)) return -1;
    if(dill_slow((first && !last) || (!first && last) || last->iol_next)) {
        errno = EINVAL; return -1;}
    return b->brecvl(b, first, last, deadline);
}

