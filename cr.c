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

#include <errno.h>
#include <stddef.h>
#include <stdio.h>

#if defined DILL_VALGRIND
#include <valgrind/valgrind.h>
#endif

#include "cr.h"
#include "debug.h"
#include "handle.h"
#include "libdill.h"
#include "poller.h"
#include "stack.h"
#include "utils.h"

static const int dill_cr_type_placeholder = 0;
static const void *dill_cr_type = &dill_cr_type_placeholder;

static void dill_cr_close(int h);
static void dill_cr_dump(int h);

static const struct hvfptrs dill_cr_vfptrs = {
    dill_cr_close,
    dill_cr_dump
};

volatile int dill_unoptimisable1 = 1;
volatile void *dill_unoptimisable2 = NULL;

struct dill_cr dill_main_data = {DILL_SLIST_ITEM_INITIALISER};
struct dill_cr *dill_main = &dill_main_data;
struct dill_cr *dill_running = &dill_main_data;

/* Queue of coroutines scheduled for execution. */
static struct dill_slist dill_ready = {0};

int dill_suspend(dill_unblock_cb unblock_cb) {
    /* Even if process never gets idle, we have to process external events
       once in a while. The external signal may very well be a deadline or
       a user-issued command that cancels the CPU intensive operation. */
    static int counter = 0;
    if(counter >= 103) {
        dill_wait(0);
        counter = 0;
    }
    /* Store the context of the current coroutine, if any. */
    if(dill_running) {
        dill_running->unblock_cb = unblock_cb;
        if(sigsetjmp(dill_running->ctx,0))
            return dill_running->sresult;
    }
    while(1) {
        /* If there's a coroutine ready to be executed go for it. */
        if(!dill_slist_empty(&dill_ready)) {
            ++counter;
            struct dill_slist_item *it = dill_slist_pop(&dill_ready);
            dill_running = dill_cont(it, struct dill_cr, ready);
            siglongjmp(dill_running->ctx, 1);
        }
        /* Otherwise, we are going to wait for sleeping coroutines
           and for external events. */
        dill_wait(1);
        dill_assert(!dill_slist_empty(&dill_ready));
        counter = 0;
    }
}

void dill_resume(struct dill_cr *cr, int result) {
    dill_assert(!dill_slist_item_inlist(&cr->ready));
    if(cr->unblock_cb) {
        cr->unblock_cb(cr);
        cr->unblock_cb = NULL;
    }
    cr->sresult = result;
    dill_slist_push_back(&dill_ready, &cr->ready);
}

/* The intial part of go(). Allocates a new stack and handle. */
int dill_prologue(sigjmp_buf **ctx, const char *created) {
    /* Ensure that debug functions are available whenever a single go()
       statement is present in the user's code. */
    dill_preserve_debug();
    /* Allocate and initialise new stack. */
    size_t stack_size;
    struct dill_cr *cr = ((struct dill_cr*)dill_allocstack(&stack_size)) - 1;
    if(dill_slow(!cr)) return -1;
    cr->hndl = dill_handle(dill_cr_type, cr, &dill_cr_vfptrs, created);
    if(dill_slow(cr->hndl < 0)) {dill_freestack(cr); errno = ENOMEM; return -1;}
    dill_slist_item_init(&cr->ready);
    cr->canceled = 0;
    cr->stopping = 0;
    cr->done = 0;
    cr->waiter = NULL;
    cr->cls = NULL;
    cr->unblock_cb = NULL;
#if defined DILL_VALGRIND
    cr->sid = VALGRIND_STACK_REGISTER((char*)(cr + 1) - stack_size, cr);
#endif
    /* Suspend the parent coroutine and make the new one running. */
    *ctx = &dill_running->ctx;
    dill_resume(dill_running, 0);    
    dill_running = cr;
    return cr->hndl;
}

/* The final part of go(). Cleans up after the coroutine is finished. */
void dill_epilogue(void) {
    /* Resume a coroutine stuck in hclose(). */
    if(dill_running->waiter)
        dill_resume(dill_running->waiter, 0);
    dill_running->done = 1;
    /* This call will never return. */
    dill_suspend(NULL);
}

static void dill_cr_close(int h) {
    struct dill_cr *cr = (struct dill_cr*)hdata(h, dill_cr_type);
    /* If the coroutine have already finished, we are done. */
    if(!cr->done) {
        /* Ask coroutine to cancel. */
        cr->canceled = 1;
        if(!dill_slist_item_inlist(&cr->ready))
            dill_resume(cr, -ECANCELED);
        /* Wait till it finishes cancelling. */
        cr->waiter = dill_running;
        int rc = dill_suspend(NULL);
        dill_assert(rc == 0);
    }
    /* Now the coroutine is finished. Deallocate it. */
    dill_freestack(cr + 1);
}

static void dill_cr_dump(int h) {
    struct dill_cr *cr = (struct dill_cr*)hdata(h, dill_cr_type);
    if(!cr) {
        fprintf(stderr, "  COROUTINE state:finished\n");
        return;
    }
    fprintf(stderr, "  COROUTINE state:running\n");
}

int dill_yield(const char *current) {
    if(dill_slow(dill_running->canceled || dill_running->stopping)) {
        errno = ECANCELED; return -1;}
    /* This looks fishy, but yes, we can resume the coroutine even before
       suspending it. */
    dill_resume(dill_running, 0);
    int rc = dill_suspend(NULL);
    if(rc == 0)
        return 0;
    dill_assert(rc < 0);
    errno = -rc;
    return -1;
}

void *cls(void) {
    return dill_running->cls;
}

void setcls(void *val) {
    dill_running->cls = val;
}

void dill_cr_postfork(void) {
    /* Drop all coroutines in the "ready to execute" list. */
    dill_slist_init(&dill_ready);
}

void dill_cr_close_main(void) {
    dill_main->canceled = 1;
    if(!dill_slist_item_inlist(&dill_main->ready))
        dill_resume(dill_main, -ECANCELED);
}

