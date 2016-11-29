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
#if (__STDC_VERSION__ >= 201112L) && (__STDC_NO_THREADS__ != 1)
#define DILL_THREAD_LOCAL _Thread_local
#elif defined __GNUC__
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

extern DILL_THREAD_LOCAL struct dill_ctx dill_context;

#endif

