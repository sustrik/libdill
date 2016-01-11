/*

  Copyright (c) 2015 Martin Sustrik

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

#include <errno.h>
#include <stddef.h>
#include <stdio.h>

#include "cr.h"
#include "debug.h"
#include "poller.h"
#include "stack.h"
#include "treestack.h"
#include "utils.h"

volatile int ts_unoptimisable1 = 1;
volatile void *ts_unoptimisable2 = NULL;

struct ts_cr ts_main = {0};

struct ts_cr *ts_running = &ts_main;

/* Queue of coroutines scheduled for execution. */
static struct ts_slist ts_ready = {0};

int ts_suspend(void) {
    /* Even if process never gets idle, we have to process external events
       once in a while. The external signal may very well be a deadline or
       a user-issued command that cancels the CPU intensive operation. */
    static int counter = 0;
    if(counter >= 103) {
        ts_wait(0);
        counter = 0;
    }
    /* Store the context of the current coroutine, if any. */
    if(ts_running && ts_setjmp(&ts_running->ctx))
        return ts_running->result;
    while(1) {
        /* If there's a coroutine ready to be executed go for it. */
        if(!ts_slist_empty(&ts_ready)) {
            ++counter;
            struct ts_slist_item *it = ts_slist_pop(&ts_ready);
            ts_running = ts_cont(it, struct ts_cr, ready);
            ts_jmp(&ts_running->ctx);
        }
        /* Otherwise, we are going to wait for sleeping coroutines
           and for external events. */
        ts_wait(1);
        ts_assert(!ts_slist_empty(&ts_ready));
        counter = 0;
    }
}

void ts_resume(struct ts_cr *cr, int result) {
    cr->result = result;
    cr->state = TS_READY;
    ts_slist_push_back(&ts_ready, &cr->ready);
}

/* The intial part of go(). Starts the new coroutine.
   Returns the pointer to the top of its stack. */
void *ts_prologue(const char *created) {
    /* Ensure that debug functions are available whenever a single go()
       statement is present in the user's code. */
    ts_preserve_debug();
    /* Allocate and initialise new stack. */
    struct ts_cr *cr = ((struct ts_cr*)ts_allocstack()) - 1;
    ts_register_cr(&cr->debug, created);
    cr->cls = NULL;
    cr->fd = -1;
    cr->events = 0;
    ts_trace(created, "{%d}=go()", (int)cr->debug.id);
    /* Suspend the parent coroutine and make the new one running. */
    if(ts_setjmp(&ts_running->ctx))
        return NULL;
    ts_resume(ts_running, 0);    
    ts_running = cr;
    /* Return pointer to the top of the stack. */
    return (void*)cr;
}

/* The final part of go(). Cleans up after the coroutine is finished. */
void ts_epilogue(void) {
    ts_trace(NULL, "go() done");
    ts_unregister_cr(&ts_running->debug);
    ts_freestack(ts_running + 1);
    ts_running = NULL;
    /* Given that there's no running coroutine at this point
       this call will never return. */
    ts_suspend();
}

int ts_yield(const char *current) {
    ts_trace(current, "yield()");
    ts_set_current(&ts_running->debug, current);
    /* This looks fishy, but yes, we can resume the coroutine even before
       suspending it. */
    ts_resume(ts_running, 0);
    ts_suspend();
    return 0;
}

void *cls(void) {
    return ts_running->cls;
}

void setcls(void *val) {
    ts_running->cls = val;
}

