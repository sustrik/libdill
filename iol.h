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

#ifndef DILL_IOL_INCLUDED
#define DILL_IOL_INCLUDED

#include <sys/uio.h>

#include "libdill.h"

/* Checks whether iolist is valid. Returns 0 in case of success or -1 in case
   of error. Fills in number of buffers in the list and overall number of bytes
   if requested. */
int dill_iolcheck(struct dill_iolist *first, struct dill_iolist *last,
    size_t *nbufs, size_t *nbytes);

/* Copy the iolist into an iovec. Iovec must have at least as much elements
   as the iolist, otherwise undefined behaviour ensues. The data buffers
   as such are not affected by this operation .*/
void dill_ioltoiov(struct dill_iolist *first, struct iovec *iov);

/* Trims first n bytes from the iolist. Returns the trimmed iolist. Keeps the
   original iolist unchanged. Returns 0 in the case of success, -1 is there
   are less than N bytes in the original iolist. */
int dill_ioltrim(struct dill_iolist *first, size_t n,
    struct dill_iolist *result);

/* Copies supplied bytes into the iolist. Returns 0 on success, -1 if bytes
   won't fit into the iolist. */
int dill_iolto(const void *src, size_t srclen, struct dill_iolist *first);

/* Copies supplied bytes from the iolist. Returns 0 on success, -1 is bytes
   won't fit into the buffer. */
int dill_iolfrom(void *dst, size_t dstlen, struct dill_iolist *first);

#endif

