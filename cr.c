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

#if defined DILL_VALGRIND
#include <valgrind/valgrind.h>
#endif

#include "cr.h"
#include "libdill.h"
#include "poller.h"
#include "stack.h"
#include "utils.h"

/* Storage for constants used by go() macro. */
volatile int dill_unoptimisable1 = 1;
volatile void *dill_unoptimisable2 = NULL;

/* Main coroutine. */
struct dill_cr dill_main_data = {DILL_SLIST_ITEM_INITIALISER};
struct dill_cr *dill_main = &dill_main_data;

/* Currently running coroutine. */
struct dill_cr *dill_r = &dill_main_data;

/* List of coroutines ready for execution. */
struct dill_slist dill_ready = {0};

/******************************************************************************/
/*  Helpers.                                                                  */
/******************************************************************************/

void dill_resume(struct dill_cr *cr, int id, int err) {
    cr->id = id;
    cr->err = err;
    dill_slist_push_back(&dill_ready, &cr->ready);
}

int dill_canblock(void) {
    if(dill_r->no_blocking1 || dill_r->no_blocking2) {
        errno = ECANCELED;
        return -1;
    }
    return 0;
}

int dill_no_blocking2(int val) {
    int old = dill_r->no_blocking2;
    dill_r->no_blocking2 = val;
    return old;
}

/******************************************************************************/
/*  Handle implementation.                                                    */
/******************************************************************************/

static const int dill_cr_type_placeholder = 0;
static const void *dill_cr_type = &dill_cr_type_placeholder;

static void dill_cr_close(int h);
static void dill_cr_dump(int h) {dill_assert(0);}

static const struct hvfptrs dill_cr_vfptrs = {
    dill_cr_close,
    dill_cr_dump
};

/******************************************************************************/
/*  Creation and termination of coroutines.                                   */
/******************************************************************************/

/* The intial part of go(). Allocates a new stack and handle. */
int dill_prologue(sigjmp_buf **ctx, const char *created) {
    /* Allocate and initialise new stack. */
    size_t stacksz;
    struct dill_cr *cr = ((struct dill_cr*)dill_allocstack(&stacksz)) - 1;
    if(dill_slow(!cr)) return -1;
    int hndl = dill_handle(dill_cr_type, cr, &dill_cr_vfptrs, created);
    if(dill_slow(hndl < 0)) {dill_freestack(cr); errno = ENOMEM; return -1;}
    dill_slist_item_init(&cr->ready);
    dill_slist_init(&cr->clauses);
    cr->closer = NULL;
    cr->cls = NULL;
#if defined DILL_VALGRIND
    cr->sid = VALGRIND_STACK_REGISTER((char*)(cr + 1) - stacksz, cr);
#endif
    /* Return the context of the parent coroutine to the caller so that it can
       store its current state. It can't be done here becuse we are at the
       wrong stack frame here. */
    *ctx = &dill_r->ctx;
    /* Add parent coroutine to the list of coroutines ready for execution. */
    dill_resume(dill_r, 0, 0);
    /* Mark the new coroutine as running. */
    dill_r = cr;
    return hndl;
}

/* The final part of go(). Gets called one the coroutine is finished. */
void dill_epilogue(void) {
    /* Mark the coroutine as finished. */
    dill_r->done = 1;
    /* If there's a coroutine waiting till we finish, unblock it now. */
    if(dill_r->closer)
        dill_cancel(dill_r->closer, 0);
    /* With no clauses added, this call will never return. */
    dill_assert(dill_slist_empty(&dill_r->clauses));
    dill_wait();
}

/* Gets called when coroutine handle is closed. */
static void dill_cr_close(int h) {
    struct dill_cr *cr = (struct dill_cr*)hdata(h, dill_cr_type);
    /* If the coroutine have already finished, we are done. */
    if(!cr->done) {
        /* No blocking calls from this point on. */
        cr->no_blocking1 = 1;
        /* Resume the coroutine if it was blocked. */
        if(!dill_slist_item_inlist(&cr->ready))
            dill_cancel(cr, -ECANCELED);
        /* Wait till the coroutine finishes execution. With no clauses added
           the only mechanism to resume is dill_cancel(). */
        /* TODO: What happens if another coroutine cancels the closer? */
        cr->closer = dill_r;
        int rc = dill_wait();
        dill_assert(rc == -1 && errno == 0);
    }
    /* Now that the coroutine is finished deallocate it. */
    dill_freestack(cr + 1);
}

/******************************************************************************/
/*  Suspend/resume functionality.                                             */
/******************************************************************************/

void dill_waitfor(struct dill_clause *cl, int id) {
    cl->cr = dill_r;
    dill_slist_item_init(&cl->item);
    dill_slist_push_back(&dill_r->clauses, &cl->item);
    cl->id = id;
}

int dill_wait(void)  {
    /* Even if process never gets idle, we have to process external events
       once in a while. The external signal may very well be a deadline or
       a user-issued command that cancels the CPU intensive operation. */
    static int counter = 0;
    /* 103 is a prime. That way it's less likely to coincide with some kind
       of cycle in the user's code. */
    if(counter >= 103) {
        dill_poller_wait(0);
        counter = 0;
    }
    /* Store the context of the current coroutine, if any. */
    if(dill_r) {
        if(sigsetjmp(dill_r->ctx,0)) {
            /* We get here once the coroutine is resumed. */
            errno = dill_r->err;
            return dill_r->id;
        }
    }
    while(1) {
        /* If there's a coroutine ready to be executed jump to it. */
        if(!dill_slist_empty(&dill_ready)) {
            ++counter;
            struct dill_slist_item *it = dill_slist_pop(&dill_ready);
            dill_r = dill_cont(it, struct dill_cr, ready);
            siglongjmp(dill_r->ctx, 1);
        }
        /* Otherwise, we are going to wait for sleeping coroutines
           and for external events. */
        dill_poller_wait(1);
        /* Sanity check: External events must have unblocked at least
           one coroutine. */
        dill_assert(!dill_slist_empty(&dill_ready));
        counter = 0;
    }
}

static void dill_docancel(struct dill_cr *cr, int id, int err) {
    /* Sanity check: Make sure that the coroutine was really suspended. */
    dill_assert(!dill_slist_item_inlist(&cr->ready));
    /* Remove the clauses from endpoints' lists of waiting coroutines. */
    struct dill_slist_item *it;
    for(it = dill_slist_begin(&cr->clauses); it; it = dill_slist_next(it)) {
        struct dill_clause *cl = dill_cont(it, struct dill_clause, item);
        if(cl->eplist)
            dill_list_erase(cl->eplist, &cl->epitem);
    }
    /* Schedule the newly unblocked coroutine for execution. */
    dill_resume(cr, id, err);
}

void dill_trigger(struct dill_clause *cl, int err) {
    dill_docancel(cl->cr, cl->id, err);
}

void dill_cancel(struct dill_cr *cr, int err) {
    dill_docancel(cr, -1, err);
}

int dill_yield(const char *current) {
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Put the current coroutine into the ready queue. */
    dill_resume(dill_r, 0, 0);
    /* Suspend. */
    return dill_wait();
}

/******************************************************************************/
/*  Coroutine-local storage.                                                  */
/******************************************************************************/

void *cls(void) {
    return dill_r->cls;
}

void setcls(void *val) {
    dill_r->cls = val;
}

