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

#ifndef DILL_STACK_INCLUDED
#define DILL_STACK_INCLUDED

#include <stddef.h>
#include <stdlib.h>

#include "libdill.h"
#include "utils.h"

extern struct alloc_vfs *dill_avfs;
extern int dill_ah;

/* Stack size in bytes. */
#define DILL_STACK_SIZE (256 * 1024)

/* Maximum number of unused cached stacks. */
#define DILL_MAX_CACHED_STACKS 64

void dill_stack_atexit(void);

/* Allocates new stack. Returns pointer to the *top* of the stack.
   For now we assume that the stack grows downwards. */
static inline void *dill_allocstack(size_t *stack_size) {
#if defined DILL_VALGRIND
    /* When using valgrind we want to deallocate cached stacks when
       the process is terminated so that they don't show up in the output. */
    static int initialized = 0;
    if(dill_slow(!initialized)) {
        int rc = atexit(dill_stack_atexit);
        dill_assert(rc == 0);
        initialized = 1;
    }
#endif
    if(dill_slow(!dill_avfs)) {
#if defined DILL_NOGUARD
        dill_ah = abasic(DILL_ALLOC_FLAGS_DEFAULT, DILL_STACK_SIZE);
#else
        dill_ah = acache(DILL_ALLOC_FLAGS_GUARD, DILL_STACK_SIZE,
            DILL_MAX_CACHED_STACKS);
#endif
        dill_avfs = hquery(dill_ah, alloc_type);
    }
    /* Allocate and initialise new stack. */
    return dill_avfs->alloc(dill_avfs);
}

/* Deallocates a stack. The argument is pointer to the top of the stack. */
static inline void dill_freestack(void *stack) {
    dill_avfs->free(dill_avfs, stack);
}

#endif
