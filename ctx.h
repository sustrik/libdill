/*

  Copyright (c) 2016 Tai Chi Minh Ralph Eastwood

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

#ifndef DILL_CONTEXT_INCLUDED
#define DILL_CONTEXT_INCLUDED

#if defined DILL_THREADS

#include <limits.h>

#include "cr.h"
#include "handle.h"
#include "pollset.h"
#include "stack.h"

/* Thread local storage support */
#if defined __GNUC__
#define DILL_THREAD_LOCAL __thread
#else
#error "No TLS support"
#endif

#else

#define DILL_THREAD_LOCAL

#endif

/* The context is statically allocated in single-threaded builds, dynamically
   allocated in multi-threaded shared builds using malloc, and allocated using
   multiple thread locals in multi-threaded static builds.

   - Single-threaded: Making global variables enables the compiler to remove
     the extra level of indirection in the case of the single-threaded build.
   - Multi-threaded(shared): Having a single thread local enables the compiler
     to group TLS accesses (reduces __tls_get_addr calls.)
   - Multi-threaded(static): Having context split into multiple thread locals
     enables the compiler to optimise TLS accesses into the least number of
     instructions (negligible difference in performance from single-threaded
     build). */

/* On OS X, builtin thread local storage through __thread does not seem
   to map to the same memory location as the TLS it is destroying.
   Thus we need to pass the context pointer through the destructors into the
   individual atexit functions.  This has been made a generalised function
   that can be utilised in both multi-threaded and single-threaded code. */
typedef void (*dill_atexit_fn)(void *ptr);

int dill_atexit(dill_atexit_fn f, void *ptr);

#define DILL_ATEXIT_MAX 16
struct dill_atexit_pair {
    dill_atexit_fn fn;
    void *ptr;
};
struct dill_atexit_fn_list {
    int count;
    struct dill_atexit_pair pair[DILL_ATEXIT_MAX];
};

#if defined(DILL_THREADS) && defined(DILL_SHARED)
struct dill_ctx {
    struct dill_ctx_cr *cr;
    struct dill_ctx_handle *handle;
    struct dill_ctx_stack *stack;
    struct dill_ctx_pollset *pollset;
    struct dill_atexit_fn_list fn_list;
};

/* This is necessary to group TLS accesses in multi-threaded shared builds. */
extern DILL_THREAD_LOCAL struct dill_ctx dill_context;
#endif
#endif

