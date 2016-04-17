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

#ifndef DILL_CR_INCLUDED
#define DILL_CR_INCLUDED

#include <setjmp.h>

#include "slist.h"

/* The coroutine. The memory layout looks like this:
   +-------------------------------------------------------------+---------+
   |                                                      stack  | dill_cr |
   +-------------------------------------------------------------+---------+
   - dill_cr contains generic book-keeping info about the coroutine
   - stack is a standard C stack; it grows downwards (at the moment libdill
     doesn't support microarchitectures where stack grows upwards)
*/
struct dill_cr {
    /* When coroutine is suspended 'ctx' holds the context
       (registers and such). */
    sigjmp_buf ctx;
    /* When coroutine is ready for execution but not running yet,
       it lives in this list (dill_ready). 'id' is a result value to return
       from dill_wait() once the coroutine is resumed. Additionally, errno
       will be set to value of 'err'. */
    struct dill_slist_item ready;
    int id;
    int err;
    /* If coroutine is blocked, here's the list of clauses it waits for. */
    struct dill_slist clauses;
    /* Coroutine-local storage. */
    void *cls;
#if defined DILL_VALGRIND
    /* Valgrind stack identifier. This way valgrind knows which areas of
       memory are used as a stacks and doesn't produce spurious warnings.
       Well, sort of. The mechanism is not perfect, but it's still better
       than nothing. */
    int sid;
#endif
};

struct dill_clause {
    /* See dill_cr's 'clauses' field. */
    struct dill_slist_item item;
    /* Number to return from dill_wait() if this clause triggers. */
    int id;
};

/* When dill_wait() is called next time, the coroutine will wait
   (among other clauses) for this clause. 'id' must not be negative. */
void dill_waitfor(struct dill_clause *cl, int id);

/* Suspend running coroutine. Move to executing different coroutines.
   The coroutine will be resumed once one of the clauses previously added by
   dill_waitfor() is triggered or dill_cancel() is called. Once that happens
   all the clauses, whether triggered or not, will be canceled. If there was no
   clause to wait for the coroutine will be suspended forever. Function returns
   ID of the triggered clause or -1 in case dill_cancel() was called. In either
   case it sets errno to the value supplied in dill_trigger/dill_cancel. */
int dill_wait(void);

/* Following two functions schedule previously suspended coroutine for
   execution. Keep in mind that they don't immediately run it, just put it into
   the queue of ready coroutines. First one will cause dill_wait() return
   the id supplied in dill_waitfor(). Second one will cause it to return -1.
   'err' is used to set errno in the resumed coroutine. */
void dill_trigger(struct dill_clause *cl, int err);
void dill_cancel(struct dill_cr *cr, int err);

#endif

