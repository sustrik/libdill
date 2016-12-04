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

#ifndef DILL_CTX_INCLUDED
#define DILL_CTX_INCLUDED

#include "cr.h"
#include "handle.h"
#include "pollset.h"
#include "stack.h"

struct dill_ctx {
#if !defined DILL_THREAD_FALLBACK
    int initialized;
#endif
    struct dill_ctx_cr cr;
    struct dill_ctx_handle handle;
    struct dill_ctx_stack stack;
    struct dill_ctx_pollset pollset;
};

struct dill_ctx *dill_ctx_init(void);

#if !defined DILL_THREADS

extern struct dill_ctx dill_ctx_;
#define dill_getctx \
    (dill_fast(dill_ctx_.initialized) ? &dill_ctx_ : dill_ctx_init())

#elif defined __GNUC__ && !defined DILL_THREAD_FALLBACK

extern __thread struct dill_ctx dill_ctx_;
#define dill_getctx \
    (dill_fast(dill_ctx_.initialized) ? &dill_ctx_ : dill_ctx_init())

#else

struct dill_ctx *dill_getctx_(void);
#define dill_getctx (dill_getctx_())

#endif

#endif

