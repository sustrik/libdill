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
#include <string.h>
#include <sys/mman.h>

#include "utils.h"

/* Returns smallest value greater than val that is a multiple of unit. */
#define dill_align(val, unit) ((val) % (unit) ?\
    (val) + (unit) - (val) % (unit) : (val))

/* Returns a size that is one page larger */
#define dill_page_larger(s,pgsz) dill_align(s + pgsz, pgsz)

#if __STDC_VERSION__ >= 201112L && defined HAVE_ALIGNED_ALLOC
#define dill_memalign(pgsz,s) aligned_alloc(pgsz, s)
#elif _POSIX_VERSION >= 200112L && defined HAVE_POSIX_MEMALIGN
static inline void *dill_memalign(size_t pgsz, size_t s) {
    void *m = NULL;
    int rc = posix_memalign(&m, pgsz, s);
    if(dill_slow(rc != 0)) {
        errno = rc;
        m = NULL;
    }
    return m;
}
#elif defined HAVE_MALLOC_H
#include <malloc.h>
#if defined HAVE_VALLOC
#define dill_memalign(pgsz,s) valloc(pgsz, s)
#elif HAVE_MEMALIGN
#define dill_memalign(pgsz,s) memalign(pgsz, s)
#elif defined HAVE_PVALLOC
#define dill_memalign(pgsz,s) pvalloc(pgsz, s)
#else
static inline void *dill_memalign(size_t pgsz, size_t s) {
    void *m = malloc(s + pgsz);
    void *ptr = ((char *)m + pgsz) & ~(pgsz - 1);
}
#endif
#endif

#define dill_malloc(p,s) malloc(s);
#define dill_calloc(p,s) calloc(s, 1);

#define dill_alloc(method, zero, pgsz, sz) ({\
    void *stack = dill_ ## method(pgsz, sz);\
    if(zero) memset(stack, 0, sz);\
    stack;\
})

static inline int dill_guard(void *ptr, size_t pgsz) {
#if HAVE_MPROTECT
    /* The bottom page is used as a stack guard. This way stack overflow will
       cause segfault rather than randomly overwrite the heap. */
    int rc = mprotect(ptr, pgsz, PROT_NONE);
    if(dill_slow(rc != 0)) {
        errno = rc;
        return -1;
    }
#endif
    return 0;
}

static inline int dill_unguard(void *ptr, size_t pgsz) {
    int rc = 0;
#if HAVE_MPROTECT
    rc = mprotect(ptr, pgsz, PROT_READ|PROT_WRITE);
#endif
    return rc;
}

size_t dill_page_size(void);

#endif

