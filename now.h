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

#ifndef DILL_NOW_INCLUDED
#define DILL_NOW_INCLUDED

#include <stdint.h>

#if defined __APPLE__
#include <mach/mach_time.h>
#endif

struct dill_ctx_now {
#if defined __APPLE__
    mach_timebase_info_data_t mtid;
#endif
#if defined(__x86_64__) || defined(__i386__)
    int64_t last_time;
    uint64_t last_tsc;
#endif
};

int dill_ctx_now_init(struct dill_ctx_now *ctx);
void dill_ctx_now_term(struct dill_ctx_now *ctx);

/* Same as dill_now() except that it doesn't use the context.
   I.e. it can be called before calling dill_ctx_now_init(). */
int64_t dill_mnow(void);

#endif

