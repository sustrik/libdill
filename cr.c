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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined DILL_VALGRIND
#include <valgrind/valgrind.h>
#endif

#include "cr.h"
#include "pollset.h"
#include "stack.h"
#include "utils.h"
#include "ctx.h"

/******************************************************************************/
/*  Autorelease pool fix.                                                     */
/******************************************************************************/

/* On darwin most foundation reference types leak memory due to a lack of a autorelease
 pool. Because of this we need to push and pop the pool frequently during libmill's
 context switches. This dynamically finds the objc runtime functions and inserts
 the push/pop calls appropriately. If any part of the rest of the application adds its own
 autorelease pool it must do so that it does not span across a libmill context switch
 otherwise the objc runtime will throw a fatal error. */
#ifdef __APPLE__
#include <dlfcn.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <string.h>

static void *autoreleasePool = NULL;
static int runningTests = 0;
static int darwinPrepared = 0;

static Class (*objc_getClass_fptr)(const char *name);
static id (*objc_msgSend_fptr)(id self, SEL, ...);
static SEL (*sel_getUid_fptr)(const char *str);

static void *(*objc_autoreleasePoolPush_fptr)(void);
static void (*objc_autoreleasePoolPop_fptr)(void *ctx);

/* Prepare the darwin environment, this mainly looks up Objective-C runtime
 functions and also determines whether or not the process is running from
 XCTest. The autorelease pools will only be added if not running in XCTest
 because XCTest runs it's own autorelease pool around each test and if we
 insert our own we corrupt the runtime. It's hard to fix because of how
 libmill switches contexts. */
void darwin_prepare() {
    if (darwinPrepared) {
        return;
    }
    
    void *handle = dlopen(NULL, RTLD_LAZY | RTLD_GLOBAL);
    
    if (handle) {
        /* Using the dlsym find the objc runtime functions we need */
        objc_getClass_fptr = dlsym(handle, "objc_getClass");
        objc_msgSend_fptr = dlsym(handle, "objc_msgSend");
        sel_getUid_fptr = dlsym(handle, "sel_getUid");
        objc_autoreleasePoolPush_fptr = dlsym(handle, "objc_autoreleasePoolPush");
        objc_autoreleasePoolPop_fptr = dlsym(handle, "objc_autoreleasePoolPop");
        
        darwinPrepared = 1;
    }
    
    /* This is a really elogated way to determine if we're running inside of XCTest.
     Because some users may accidentally import XCTest into their binary we don't
     just want to do class detection.
     
     This grabs [[[[NSProcessInfo processInfo] arguments] description] UTF8String]
     and searches it for /Xcode/Agents/xctest OR /usr/bin/xctest OR PackageTests.xctest
     in the future these searches may not be valid and need to be tweaked. */
    Class processInfoClass = objc_getClass_fptr("NSProcessInfo");
        
    if (processInfoClass) {
        id processInfo = objc_msgSend_fptr((id)processInfoClass, sel_getUid_fptr("processInfo"));
        id arguments = objc_msgSend_fptr(processInfo, sel_getUid_fptr("arguments"));
        id description = objc_msgSend_fptr(arguments, sel_getUid_fptr("description"));
        const char *chars = (const char *)objc_msgSend_fptr(description, sel_getUid_fptr("UTF8String"));
        
        runningTests = (strstr(chars, "/Xcode/Agents/xctest") ||
                        strstr(chars, "/usr/bin/xctest") ||
                        strstr(chars, "PackageTests.xctest"));
    }
    
    if (!darwinPrepared) {
        printf("libmill failed to prepare for Darwin platform.");
        dill_assert(0);
    }
}

static inline void darwin_pool_push() {
    darwin_prepare();
    
    if (runningTests) {
        return;
    }
    
    dill_assert(!autoreleasePool);
    autoreleasePool = objc_autoreleasePoolPush_fptr();
}

static inline void darwin_pool_pop() {
    darwin_prepare();
    
    if (runningTests) {
        return;
    }
    
    if (autoreleasePool) {
        objc_autoreleasePoolPop_fptr(autoreleasePool);
        autoreleasePool = NULL;
    }
}

static inline void darwin_pool_pop_push() {
    darwin_prepare();
    
    if (runningTests) {
        return;
    }
    
    if (autoreleasePool) {
        objc_autoreleasePoolPop_fptr(autoreleasePool);
        autoreleasePool = NULL;
    }
    
    dill_assert(!autoreleasePool);
    autoreleasePool = objc_autoreleasePoolPush_fptr();
}
#endif


#if defined DILL_CENSUS

/* When taking the stack size census, we will keep the maximum stack size in a list
   indexed by the go() call, i.e., by file name and line number. */
struct dill_census_item {
    struct dill_slist crs;
    const char *file;
    int line;
    size_t max_stack;
};

#endif

/* Storage for the constant used by the go() macro. */
volatile void *dill_unoptimisable = NULL;

/******************************************************************************/
/*  Helpers.                                                                  */
/******************************************************************************/

static void dill_resume(struct dill_cr *cr, int id, int err) {
#ifdef __APPLE__
    darwin_pool_pop_push();
#endif
    
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    cr->id = id;
    cr->err = err;
    dill_qlist_push(&ctx->ready, &cr->ready);
}

int dill_canblock(void) {
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    if(dill_slow(ctx->r->no_blocking1 || ctx->r->no_blocking2)) {
        errno = ECANCELED; return -1;}
    return 0;
}

int dill_no_blocking(int val) {
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    int old = ctx->r->no_blocking2;
    ctx->r->no_blocking2 = val;
    return old;
}

/******************************************************************************/
/*  Context.                                                                  */
/******************************************************************************/

int dill_ctx_cr_init(struct dill_ctx_cr *ctx) {
    /* This function is definitely called from the main coroutine, given that
       it's called only once and you can't even create a different coroutine
       without calling it. */
    ctx->r = &ctx->main;
    dill_qlist_init(&ctx->ready);
    dill_rbtree_init(&ctx->timers);
    /* We can't use now() here as the context is still being intialized. */
    ctx->last_poll = mnow();
    /* Initialize the main coroutine. */
    memset(&ctx->main, 0, sizeof(ctx->main));
    ctx->main.ready.next = NULL;
    dill_slist_init(&ctx->main.clauses);
#if defined DILL_CENSUS
    dill_slist_init(&ctx->census);
#endif
    return 0;
}

void dill_ctx_cr_term(struct dill_ctx_cr *ctx) {
#if defined DILL_CENSUS
    struct dill_slist *it;
    for(it = dill_slist_next(&ctx->census); it != &ctx->census;
          it = dill_slist_next(it)) {
        struct dill_census_item *ci =
            dill_cont(it, struct dill_census_item, crs);
        fprintf(stderr, "%s:%d - maximum stack size %zu B\n",
            ci->file, ci->line, ci->max_stack);
    }
#endif
}

/******************************************************************************/
/*  Timers.                                                                   */
/******************************************************************************/

static void dill_timer_cancel(struct dill_clause *cl) {
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    struct dill_tmclause *tmcl = dill_cont(cl, struct dill_tmclause, cl);
    dill_rbtree_erase(&ctx->timers, &tmcl->item);
    /* This is a safeguard. If an item isn't properly removed from the rb-tree,
       we can spot the fact by seeing that the cr has been set to NULL. */
    tmcl->cl.cr = NULL;
}

/* Adds a timer clause to the list of clauses being waited on. */
void dill_timer(struct dill_tmclause *tmcl, int id, int64_t deadline) {
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    /* If the deadline is infinite, there's nothing to wait for. */
    if(deadline < 0) return;
    dill_rbtree_insert(&ctx->timers, deadline, &tmcl->item);
    dill_waitfor(&tmcl->cl, id, dill_timer_cancel);
}

/******************************************************************************/
/*  Handle implementation.                                                    */
/******************************************************************************/

static const int dill_cr_type_placeholder = 0;
static const void *dill_cr_type = &dill_cr_type_placeholder;
static void *dill_cr_query(struct hvfs *vfs, const void *type);
static void dill_cr_close(struct hvfs *vfs);

/******************************************************************************/
/*  Coroutine creation and termination                                        */
/******************************************************************************/

static void dill_cancel(struct dill_cr *cr, int err);

/* The initial part of go(). Allocates a new stack and handle. */
int dill_prologue(sigjmp_buf **jb, void **ptr, size_t len,
      const char *file, int line) {
#ifdef __APPLE__
    darwin_pool_pop_push();
#endif
    
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    /* Return ECANCELED if shutting down. */
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) {errno = ECANCELED; return -1;}
    struct dill_cr *cr;
    size_t stacksz;
    if(!*ptr) {
        /* Allocate a new stack. */
        cr = (struct dill_cr*)dill_allocstack(&stacksz);
        if(dill_slow(!cr)) return -1;
    }
    else {
        /* The stack is supplied by the user.
           Align the top of the stack to a 16-byte boundary. */
        uintptr_t top = (uintptr_t)*ptr;
        top += len;
        top &= ~(uintptr_t)15;
        stacksz = top - (uintptr_t)*ptr;
        cr = (struct dill_cr*)top;
        if(dill_slow(stacksz < sizeof(struct dill_cr))) {
            errno = ENOMEM; return -1;}
    }
#if defined DILL_CENSUS
    /* Mark the bytes in the stack as unused. */
    uint8_t *bottom = ((char*)cr) - stacksz;
    int i;
    for(i = 0; i != stacksz; ++i)
        bottom[i] = 0xa0 + (i % 13);
#endif
    --cr;
    cr->vfs.query = dill_cr_query;
    cr->vfs.close = dill_cr_close;
    cr->vfs.done = NULL;
    int hndl = hmake(&cr->vfs);
    if(dill_slow(hndl < 0)) {
        int err = errno; dill_freestack(cr + 1); errno = err; return -1;}
    cr->ready.next = NULL;
    dill_slist_init(&cr->clauses);
    cr->closer = NULL;
    cr->no_blocking1 = 0;
    cr->no_blocking2 = 0;
    cr->done = 0;
    cr->mem = *ptr ? 1 : 0;
#if defined DILL_VALGRIND
    cr->sid = VALGRIND_STACK_REGISTER((char*)(cr + 1) - stacksz, cr);
#endif
#if defined DILL_CENSUS
    /* Find the appropriate census item if it exists. It's O(n) but meh. */
    cr->census = NULL;
    struct dill_slist *it;
    for(it = dill_slist_next(&ctx->census); it != &ctx->census;
          it = dill_slist_next(it)) {
        cr->census = dill_cont(it, struct dill_census_item, crs);
        if(cr->census->line == line && strcmp(cr->census->file, file) == 0)
            break;
    }
    /* Allocate it if it does not exist. */
    if(it == &ctx->census) {
        cr->census = malloc(sizeof(struct dill_census_item));
        dill_assert(cr->census);
        dill_slist_push(&ctx->census, &cr->census->crs);
        cr->census->file = file;
        cr->census->line = line;
        cr->census->max_stack = 0;
    }
    cr->stacksz = stacksz - sizeof(struct dill_cr);
#endif
    /* Return the context of the parent coroutine to the caller so that it can
       store its current state. It can't be done here because we are at the
       wrong stack frame here. */
    *jb = &ctx->r->ctx;
    /* Add parent coroutine to the list of coroutines ready for execution. */
    dill_resume(ctx->r, 0, 0);
    /* Mark the new coroutine as running. */
    *ptr = ctx->r = cr;
    return hndl;
}

/* The final part of go(). Gets called when the coroutine is finished. */
void dill_epilogue(void) {
#ifdef __APPLE__
    darwin_pool_pop_push();
#endif
    
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    /* Mark the coroutine as finished. */
    ctx->r->done = 1;
    /* If there's a coroutine waiting for us to finish, unblock it now. */
    if(ctx->r->closer)
        dill_cancel(ctx->r->closer, 0);
    /* With no clauses added, this call will never return. */
    dill_assert(dill_slist_empty(&ctx->r->clauses));
    dill_wait();
}

static void *dill_cr_query(struct hvfs *vfs, const void *type) {
    if(dill_slow(type != dill_cr_type)) {errno = ENOTSUP; return NULL;}
    struct dill_cr *cr = dill_cont(vfs, struct dill_cr, vfs);
    return cr;
}

/* Gets called when coroutine handle is closed. */
static void dill_cr_close(struct hvfs *vfs) {
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    struct dill_cr *cr = dill_cont(vfs, struct dill_cr, vfs);
    /* If the coroutine has already finished, we are done. */
    if(!cr->done) {
        /* No blocking calls from this point on. */
        cr->no_blocking1 = 1;
        /* Resume the coroutine if it was blocked. */
        if(!cr->ready.next)
            dill_cancel(cr, ECANCELED);
        /* Wait for the coroutine to stop executing. With no clauses added,
           the only mechanism to resume is through dill_cancel(). This is not really
           a blocking call, although it looks like one. Given that the coroutine
           that is being shut down is not permitted to block, we should get
           control back pretty quickly. */
        cr->closer = ctx->r;
        int rc = dill_wait();
        dill_assert(rc == -1 && errno == 0);
    }
#if defined DILL_CENSUS
    /* Find the first overwritten byte on the stack.
       Determine stack usage based on that. */
    uint8_t *bottom = ((uint8_t*)cr) - cr->stacksz;
    int i;
    for(i = 0; i != cr->stacksz; ++i) {
        if(bottom[i] != 0xa0 + (i % 13)) {
            /* dill_cr is located on the stack so we have to take that into account.
               Also, it may be necessary to align the top of the stack to
               a 16-byte boundary, so add 16 bytes to account for that. */
            size_t used = cr->stacksz - i - sizeof(struct dill_cr) + 16;
            if(used > cr->census->max_stack)
                cr->census->max_stack = used;
            break;
        }
    }
#endif
#if defined DILL_VALGRIND
    VALGRIND_STACK_DEREGISTER(cr->sid);
#endif
    /* Now that the coroutine is finished, deallocate it. */
    if(!cr->mem) dill_freestack(cr + 1);
}

/******************************************************************************/
/*  Suspend/resume functionality.                                             */
/******************************************************************************/

void dill_waitfor(struct dill_clause *cl, int id,
      void (*cancel)(struct dill_clause *cl)) {
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    /* Add a clause to the coroutine list of active clauses. */
    cl->cr = ctx->r;
    dill_slist_push(&ctx->r->clauses, &cl->item);
    cl->id = id;
    cl->cancel = cancel;
}

int dill_wait(void)  {
#ifdef __APPLE__
    darwin_pool_pop_push();
#endif
    
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    /* Store the context of the current coroutine, if any. */
    if(dill_setjmp(ctx->r->ctx)) {
        /* We get here once the coroutine is resumed. */
        dill_slist_init(&ctx->r->clauses);
        errno = ctx->r->err;
        return ctx->r->id;
    }
    /* For performance reasons, we want to avoid excessive checking of current
       time, so we cache the value here. It will be recomputed only after
       a blocking call. */
    int64_t nw = now();
    /*  Wait for timeouts and external events. However, if there are ready
       coroutines there's no need to poll for external events every time.
       Still, we'll do it at least once a second. The external signal may
       very well be a deadline or a user-issued command that cancels the CPU
       intensive operation. */
    if(dill_qlist_empty(&ctx->ready) || nw > ctx->last_poll + 1000) {
        int block = dill_qlist_empty(&ctx->ready);
        while(1) {
            /* Compute the timeout for the subsequent poll. */
            int timeout = 0;
            if(block) {
                if(dill_rbtree_empty(&ctx->timers))
                    timeout = -1;
                else {
                    int64_t deadline = dill_cont(
                        dill_rbtree_first(&ctx->timers),
                        struct dill_tmclause, item)->item.val;
                    timeout = (int) (nw >= deadline ? 0 : deadline - nw);
                }
            }
            /* Wait for events. */
            int fired = dill_pollset_poll(timeout);
            if(timeout != 0) nw = now();
            if(dill_slow(fired < 0)) continue;
            /* Fire all expired timers. */
            if(!dill_rbtree_empty(&ctx->timers)) {
                while(!dill_rbtree_empty(&ctx->timers)) {
                    struct dill_tmclause *tmcl = dill_cont(
                        dill_rbtree_first(&ctx->timers),
                        struct dill_tmclause, item);
                    if(tmcl->item.val > nw)
                        break;
                    dill_trigger(&tmcl->cl, ETIMEDOUT);
                    fired = 1;
                }
            }
            /* Never retry the poll when in non-blocking mode. */
            if(!block || fired)
                break;
            /* If the timeout was hit but there were no expired timers, do the poll
               again. It can happen if the timers were canceled in the
               meantime. */
        }
        ctx->last_poll = nw;
    }
    /* There's a coroutine ready to be executed so jump to it. */
    struct dill_slist *it = dill_qlist_pop(&ctx->ready);
    it->next = NULL;
    ctx->r = dill_cont(it, struct dill_cr, ready);
    /* dill_longjmp has to be at the end of a function body, otherwise stack
       unwinding information will be trimmed if a crash occurs in this
       function. */
    dill_longjmp(ctx->r->ctx);
    
#ifdef __APPLE__
    darwin_pool_pop_push();
#endif
    
    return 0;
}

static void dill_docancel(struct dill_cr *cr, int id, int err) {
    /* Sanity check: Make sure that the coroutine was really suspended. */
    dill_assert(!cr->ready.next);
    /* Remove the clauses from endpoints' lists of waiting coroutines. */
    struct dill_slist *it;
    for(it = dill_slist_next(&cr->clauses); it != &cr->clauses;
          it = dill_slist_next(it)) {
        struct dill_clause *cl = dill_cont(it, struct dill_clause, item);
        if(cl->cancel) cl->cancel(cl);
    }
    /* Schedule the newly unblocked coroutine for execution. */
    dill_resume(cr, id, err);
}

void dill_trigger(struct dill_clause *cl, int err) {
    dill_docancel(cl->cr, cl->id, err);
}

static void dill_cancel(struct dill_cr *cr, int err) {
    dill_docancel(cr, -1, err);
}

int yield(void) {
    struct dill_ctx_cr *ctx = &dill_getctx->cr;
    int rc = dill_canblock();
    if(dill_slow(rc < 0)) return -1;
    /* Put the current coroutine into the ready queue. */
    dill_resume(ctx->r, 0, 0);
    /* Suspend. */
    return dill_wait();
}

int co(void **ptr, size_t len, void *fn, const char *file, int line, void (*routine)(void *)) {
    sigjmp_buf *ctx;
    void *stk = (ptr);
    int h = dill_prologue(&ctx, &stk, len, file, line);
    
    if(h >= 0) {
        if(!dill_setjmp(*ctx)) {
            DILL_SETSP(stk);
            routine(fn);
            dill_epilogue();
        }
    }
    
    return h;
}
