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

#include "libdill.h"
#include "context.h"
#include "cr.h"
#include "handle.h"
#include "stack.h"

DILL_THREAD_LOCAL struct dill_ctx dill_context = {
    .cr = &dill_ctx_cr_main_data,
    .handle = &dill_ctx_handle_main_data,
    .stack = &dill_ctx_stack_main_data,
    /* Pollset is managed by cr.c even in main thread. */
    .pollset = NULL,
};

int ctxinit(void) {
    int rc = dill_inithandle();
    if(dill_slow(rc != 0)) return rc;
    rc = dill_initstack();
    if(dill_slow(rc != 0)) return rc;
    rc = dill_initcr();
    return rc;
}

void ctxterm(void) {
    dill_termcr();
    dill_termstack();
    dill_termhandle();
}
