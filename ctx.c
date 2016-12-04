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

#include "ctx.h"

#if !defined DILL_THREADS

struct dill_ctx dill_ctx_ = {0};

static void dill_ctx_atexit(void) {
    dill_ctx_pollset_term(&dill_ctx_.pollset);
    dill_ctx_stack_term(&dill_ctx_.stack);
    dill_ctx_handle_term(&dill_ctx_.handle);
    dill_ctx_cr_term(&dill_ctx_.cr);
}

struct dill_ctx *dill_ctx_init(void) {
    int rc = dill_ctx_cr_init(&dill_ctx_.cr);
    dill_assert(rc == 0);
    rc = dill_ctx_handle_init(&dill_ctx_.handle);
    dill_assert(rc == 0);
    rc = dill_ctx_stack_init(&dill_ctx_.stack);
    dill_assert(rc == 0);
    rc = dill_ctx_pollset_init(&dill_ctx_.pollset);
    dill_assert(rc == 0);
    rc = atexit(dill_ctx_atexit);
    dill_assert(rc == 0);
    dill_ctx_.initialized = 1;
    return &dill_ctx_;
}

#elif defined __GNUC__

#error "TODO: Use __thread"

#else

#error "TODO: Fallback to pthread_getspecific()"

#endif

