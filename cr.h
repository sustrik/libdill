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
#include <stdint.h>

#include "debug.h"
#include "list.h"
#include "slist.h"
#include "timer.h"
#include "utils.h"

#define DILL_OPAQUE_SIZE 48

struct dill_cr;

typedef void (*dill_unblock_cb)(struct dill_cr *cr);

/* The coroutine. The memory layout looks like this:

   +-------------------------------------------------------------+---------+
   |                                                      stack  | dill_cr |
   +-------------------------------------------------------------+---------+

   - dill_cr contains generic book-keeping info about the coroutine
   - stack is a standard C stack; it grows downwards (at the moment libdill
     doesn't support microarchitectures where stack grows upwards)

*/
struct dill_cr {
    /* The coroutine is stored in this list if it is not blocked and it is
       waiting to be executed. */
    struct dill_slist_item ready;
    /* If the coroutine is waiting for a deadline, it uses this timer. */
    struct dill_timer timer;
    /* Handle of this coroutine. */
    int hndl;
    /* When coroutine is suspended 'ctx' holds the context (registers and such),
       'unblock_cb' is a function to be called when the coroutine is moved back
       to the list of ready coroutines and 'sresult' is the value to be returned
       from the suspend function. */
    sigjmp_buf ctx;
    dill_unblock_cb unblock_cb;
    int sresult;
    /* 1 if this corotine was stopped by its owner. */
    int canceled;
    /* 1 is execution is inside a 'stop' function. */
    int stopping;
    /* When coroutine is being waited for using hwait() this is the pointer
       to the waiting coroutine. */
    struct dill_cr *waiter;
    /* Coroutine-local storage. */
    void *cls;
#if defined DILL_VALGRIND
    /* Valgrind stack identifier. */
    int sid;
#endif
    /* Opaque storage for whatever data the current blocking operation
       needs to store while it is suspended. */
    uint8_t opaque[DILL_OPAQUE_SIZE];
};

/* Fake coroutine corresponding to the main thread of execution. */
extern struct dill_cr dill_main;

/* The coroutine that is running at the moment. */
extern struct dill_cr *dill_running;

/* Suspend running coroutine. Move to executing different coroutines. Once
   someone resumes this coroutine using dill_resume(), unblock_cb is
   invoked immediately. dill_suspend() returns the argument passed
   to dill_resume() function. */
int dill_suspend(dill_unblock_cb unblock_cb);

/* Schedules preiously suspended coroutine for execution. Keep in mind that
   it doesn't immediately run it, just puts it into the queue of ready
   coroutines. */
void dill_resume(struct dill_cr *cr, int result);

/* Called in the child process after fork to stop all the coroutines 
   inherited from the parent. */
void dill_cr_postfork(void);

#endif

