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
static int dill_cr_wait(int h, int *status, int64_t deadline);
static void dill_cr_dump(int h);

static const struct hvfptrs dill_cr_vfptrs = {
    dill_cr_close,
    dill_cr_wait,
    dill_cr_dump
};

volatile int dill_unoptimisable1 = 1;
volatile void *dill_unoptimisable2 = NULL;

struct dill_cr dill_main = {DILL_SLIST_ITEM_INITIALISER};

struct dill_cr *dill_running = &dill_main;

/* Queue of coroutines scheduled for execution. */
static struct dill_slist dill_ready = {0};

#define dill_setjmp(ctx) \
    ({\
        dill_assert(!(ctx)->valid);\
        (ctx)->valid = 1;\
        sigsetjmp((ctx)->jbuf, 0);\
    })
#define dill_jmp(ctx) \
    do{\
        dill_assert((ctx)->valid);\
        (ctx)->valid = 0;\
        siglongjmp((ctx)->jbuf, 1);\
    } while(0)

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
        if(dill_setjmp(&dill_running->ctx))
            return dill_running->sresult;
    }
    while(1) {
        /* If there's a coroutine ready to be executed go for it. */
        if(!dill_slist_empty(&dill_ready)) {
            ++counter;
            struct dill_slist_item *it = dill_slist_pop(&dill_ready);
            dill_running = dill_cont(it, struct dill_cr, ready);
            dill_jmp(&dill_running->ctx);
        }
        /* Otherwise, we are going to wait for sleeping coroutines
           and for external events. */
        dill_wait(1);
        dill_assert(!dill_slist_empty(&dill_ready));
        counter = 0;
    }
}

void dill_resume(struct dill_cr *cr, int result) {
    if(cr->unblock_cb) {
        cr->unblock_cb(cr);
        cr->unblock_cb = NULL;
    }
    cr->sresult = result;
    dill_slist_push_back(&dill_ready, &cr->ready);
}

/* dill_prologue() and dill_epilogue() live in the same scope with
   libdill's stack-switching black magic. As such, they are extremely
   fragile. Therefore, the optimiser is prohibited to touch them. */
#if defined __clang__
#define dill_noopt __attribute__((optnone))
#elif defined __GNUC__
#define dill_noopt __attribute__((optimize("O0")))
#else
#error "Unsupported compiler!"
#endif

/* The intial part of go(). Starts the new coroutine.  Returns 1 in the
   new coroutine, 0 in the old one. */
__attribute__((noinline)) dill_noopt
int dill_prologue(int *hndl, const char *created) {
    /* Ensure that debug functions are available whenever a single go()
       statement is present in the user's code. */
    dill_preserve_debug();
    /* Allocate and initialise new stack. */
    struct dill_cr *cr = ((struct dill_cr*)dill_allocstack()) - 1;
    if(dill_slow(!cr)) {*hndl = -1; return 0;}
    *hndl = handle(dill_cr_type, cr, &dill_cr_vfptrs);
    if(dill_slow(*hndl < 0)) {dill_freestack(cr); errno = ENOMEM; return 0;}
    dill_slist_item_init(&cr->ready);
    cr->hndl = *hndl;
    cr->canceled = 0;
    cr->stopping = 0;
    cr->waiter = NULL;
    cr->cls = NULL;
    cr->unblock_cb = NULL;
    cr->ctx.valid = 0;
    /* Suspend the parent coroutine and make the new one running. */
    if(dill_setjmp(&dill_running->ctx))
        return 0;
    dill_resume(dill_running, 0);    
    dill_running = cr;
    return 1;
}

/* The final part of go(). Cleans up after the coroutine is finished. */
__attribute__((noinline)) dill_noopt
void dill_epilogue(int result) {
    /* Result is stored in the handle so that it is available even after
       the stack is deallocated. */
    dill_handle_setcrresult(dill_running->hndl, result);
    /* Resume a coroutine stuck in hwait(), if any. */
    if(dill_running->waiter)
        dill_resume(dill_running->waiter, 0);
    /* Deallocate. */
    dill_freestack(dill_running + 1);
    dill_running = NULL;
    /* Given that there's no running coroutine at this point
       this call will never return. */
    dill_suspend(NULL);
}

static int dill_cr_wait(int h, int *status, int64_t deadline) {
    struct dill_cr *cr = (struct dill_cr*)hdata(h, dill_cr_type);
    /* If cr is NULL, the coroutine have already finished. */
    if(cr) {
        /* If not so and immediate deadline is requested,
           return straight away. */
        if(deadline == 0) {errno = ETIMEDOUT; return -1;}
        /* Wait till the coroutine finishes. */
        if(deadline > 0)
            dill_timer_add(&dill_running->timer, deadline);
        cr->waiter = dill_running;
        int rc = dill_suspend(NULL);
        cr->waiter = NULL;
        if(deadline > 0 && rc != -ETIMEDOUT)
            dill_timer_rm(&dill_running->timer);
        if(dill_slow(rc < 0)) {errno = -rc; return -1;}
    }
    if(status)
        *status = dill_handle_getcrresult(h);
    return 0; 
}

static void dill_cr_close(int h) {
    struct dill_cr *cr = (struct dill_cr*)hdata(h, dill_cr_type);
    /* If the coroutine have already finished, we are done. */
    if(!cr) return;
    /* Ask coroutine to cancel. */
    cr->canceled = 1;
    if(!dill_slist_item_inlist(&cr->ready))
        dill_resume(cr, -ECANCELED);
    /* Wait till it finishes cancelling. */
    int rc = dill_cr_wait(h, NULL, -1);
    dill_assert(rc == 0);
}

static void dill_cr_dump(int h) {
    struct dill_cr *cr = (struct dill_cr*)hdata(h, dill_cr_type);
    dill_assert(cr);
    fprintf(stderr, "  COROUTINE\n");
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

