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

#ifndef DILL_IOL_INCLUDED
#define DILL_IOL_INCLUDED

#include <sys/uio.h>

#include "libdill.h"

/* Checks whether iolist is valid. Returns 0 in case of success or -1 in case
   of error. Fills in number of buffers in the list and overall number of bytes
   if requested. */
int iol_check(struct iolist *first, struct iolist *last,
    size_t *nbufs, size_t *nbytes);

/* Copy the iolist into an iovec. Iovec must have at least as much elements
   as the iolist, otherwise undefined behaviour ensues. The data buffers
   as such are not affected by this operation .*/
void iol_toiov(struct iolist *first, struct iovec *iov);

#endif

