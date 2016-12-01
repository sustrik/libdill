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

#ifndef DILL_CONTEXT_INCLUDED
#define DILL_CONTEXT_INCLUDED

#if defined DILL_THREADS

#include <limits.h>

/* Thread local storage support */
#if defined __GNUC__
#define DILL_THREAD_LOCAL __thread
#else
#error "No TLS support"
#endif

#else

#define DILL_THREAD_LOCAL

#endif

struct dill_ctx_cr;
struct dill_ctx_handle;
struct dill_ctx_stack;
struct dill_ctx_pollset;

struct dill_ctx {
    struct dill_ctx_cr *cr;
    struct dill_ctx_handle *handle;
    struct dill_ctx_stack *stack;
    struct dill_ctx_pollset *pollset;
};

/* The context is statically allocated in single-threaded builds and
   dynamically allocated in multi-threaded builds.
   This enables the compiler to remove the extra level of indirection in the
   case of the single-threaded build. */
#if defined DILL_THREADS
extern DILL_THREAD_LOCAL struct dill_ctx dill_context;
#else
/* Statically declared in cr.c, handle.c, stack.c, and pollset.c */
extern struct dill_ctx_cr dill_ctx_cr_data;
extern struct dill_ctx_handle dill_ctx_handle_data;
extern struct dill_ctx_stack dill_ctx_stack_data;
extern struct dill_ctx_pollset dill_ctx_pollset_data;
static const struct dill_ctx dill_context = {
    .cr = &dill_ctx_cr_data,
    .handle = &dill_ctx_handle_data,
    .stack = &dill_ctx_stack_data,
    .pollset = &dill_ctx_pollset_data,
};
#endif

typedef void (*dill_atexit_fn)(void);

int dill_atexit(dill_atexit_fn f);

#endif

