/*

  Copyright (c) 2016 Martin Sustrik

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

#ifndef DILL_PAGE_H_INCLUDED
#define DILL_PAGE_H_INCLUDED

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

/* Returns smallest value greater than val that is a multiple of unit. */
#define dill_align(val, unit) ((val) % (unit) ?\
    (val) + (unit) - (val) % (unit) : (val))

#if __STDC_VERSION__ >= 201112L && defined HAVE_ALIGNED_ALLOC
#define dill_memalign(pgsz,s) aligned_alloc(pgsz, s)
#elif _POSIX_VERSION >= 200112L && defined HAVE_POSIX_MEMALIGN
static inline void *dill_memalign_(size_t pgsz, size_t s) {
    void *m = NULL;
    int rc = posix_memalign(&m, pgsz, s);
    if(dill_slow(rc != 0)) {
        errno = rc;
        m = NULL;
    }
    return m;
}
#define dill_memalign(pgsz,s) dill_memalign_(pgsz, s)
#elif defined HAVE_MALLOC_H
#include <malloc.h>
#if defined HAVE_VALLOC
#define dill_memalign(pgsz,s) valloc(pgsz, s)
#elif HAVE_MEMALIGN
#define dill_memalign(pgsz,s) memalign(pgsz, s)
#elif defined HAVE_PVALLOC
#define dill_memalign(pgsz,s) pvalloc(pgsz, s)
#else
#endif
#endif

#ifndef dill_memalign
#error "Target system does not have posix_memalign compatible function!"
#endif

size_t dill_page_size(void);

#endif

