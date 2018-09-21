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

static void dill_ctx_init_(struct dill_ctx *ctx) {
    dill_assert(ctx->initialized == 0);
    ctx->initialized = 1;
    int rc = dill_ctx_now_init(&ctx->now);
    dill_assert(rc == 0);
    rc = dill_ctx_cr_init(&ctx->cr);
    dill_assert(rc == 0);
    rc = dill_ctx_handle_init(&ctx->handle);
    dill_assert(rc == 0);
    rc = dill_ctx_stack_init(&ctx->stack);
    dill_assert(rc == 0);
    rc = dill_ctx_pollset_init(&ctx->pollset);
    dill_assert(rc == 0);
    rc = dill_ctx_fd_init(&ctx->fd);
    dill_assert(rc == 0);
}

static void dill_ctx_term_(struct dill_ctx *ctx) {
    dill_assert(ctx->initialized == 1);
    dill_ctx_fd_term(&ctx->fd);
    dill_ctx_pollset_term(&ctx->pollset);
    dill_ctx_stack_term(&ctx->stack);
    dill_ctx_handle_term(&ctx->handle);
    dill_ctx_cr_term(&ctx->cr);
    dill_ctx_now_term(&ctx->now);
    ctx->initialized = 0;
}

#if !defined DILL_THREADS

/* This implementation of context is used when threading is disabled, i.e.
   when user wishes to have a single-threaded application. Context is stored
   in a global variable which is deallocated using an atexit() function. */

struct dill_ctx dill_ctx_ = {0};

static void dill_ctx_atexit(void) {
    dill_ctx_term_(&dill_ctx_);
}

struct dill_ctx *dill_ctx_init(void) {
    dill_ctx_init_(&dill_ctx_);
    int rc = atexit(dill_ctx_atexit);
    dill_assert(rc == 0);
    return &dill_ctx_;
}

#else

#include <pthread.h>

/* Determine whether current thread is the main thread. */
#if defined __linux__
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
static int dill_ismain() {
    return syscall(SYS_gettid) == getpid();
}
#elif defined __OpenBSD__ || defined __FreeBSD__ || \
    defined __APPLE__ || defined __DragonFly__
#if defined __FreeBSD__ || defined __OpenBSD__
#include <pthread_np.h>
#endif
static int dill_ismain() {
    return pthread_main_np();
}
#elif defined __NetBSD__
#include <sys/lwp.h>
static int dill_ismain() {
    return _lwp_self() == 1;
}
#elif defined __sun
static int dill_ismain() {
    return pthread_self() == 1;
}
#else
#error "Cannot determine which thread is the main thread."
#endif

#if defined __GNUC__ && !defined __APPLE__ && !defined DILL_THREAD_FALLBACK

/* This implementation is used if the compiler supports thread-local variables.
   The context is stored in a thread-local variable. It uses pthread_key_create
   to register a destructor function which cleans the context up after thread
   exits. However, at least on Linux, the destructor is not called for the
   main thread. Therefore, atexit() function is registered to clean the
   main thread's context. */

__thread struct dill_ctx dill_ctx_ = {0};

static pthread_key_t dill_key;
static pthread_once_t dill_keyonce = PTHREAD_ONCE_INIT;
static void *dill_main = NULL;

static void dill_ctx_term(void *ptr) {
    struct dill_ctx *ctx = ptr;
    dill_ctx_term_(ctx);
    if(dill_ismain()) dill_main = NULL;
}

static void dill_ctx_atexit(void) {
    if(dill_main) dill_ctx_term(dill_main);
}

static void dill_makekey(void) {
    int rc = pthread_key_create(&dill_key, dill_ctx_term);
    dill_assert(!rc);
}

struct dill_ctx *dill_ctx_init(void) {
    dill_ctx_init_(&dill_ctx_);
    int rc = pthread_once(&dill_keyonce, dill_makekey);
    dill_assert(rc == 0);
    if(dill_ismain()) {
        dill_main = &dill_ctx_;
        rc = atexit(dill_ctx_atexit);
        dill_assert(rc == 0);
    }
    rc = pthread_setspecific(dill_key, &dill_ctx_);
    dill_assert(rc == 0);
    return &dill_ctx_;
}

#else

/* This is an implementation that doesn't need compiler support for thread-local
   variables. It creates thread-specific data usin POSIX functions.
   It uses pthread_key_create to register a destructor function which cleans
   the context up after thread exits. However, at least on Linux, the destructor
   is not called for the main thread. Therefore, atexit() function is registered
   to clean the main thread's context. */

static pthread_key_t dill_key;
static pthread_once_t dill_keyonce = PTHREAD_ONCE_INIT;
static void *dill_main = NULL;

static void dill_ctx_term(void *ptr) {
    struct dill_ctx *ctx = ptr;
    dill_ctx_term_(ctx);
    free(ctx);
    if(dill_ismain()) dill_main = NULL;
}

static void dill_ctx_atexit(void) {
    if(dill_main) dill_ctx_term(dill_main);
}

static void dill_makekey(void) {
    int rc = pthread_key_create(&dill_key, dill_ctx_term);
    dill_assert(!rc);
}

struct dill_ctx *dill_getctx_(void) {
    int rc = pthread_once(&dill_keyonce, dill_makekey);
    dill_assert(rc == 0);
    struct dill_ctx *ctx = pthread_getspecific(dill_key);
    if(dill_fast(ctx)) return ctx;
    ctx = calloc(1, sizeof(struct dill_ctx));
    dill_assert(ctx);
    dill_ctx_init_(ctx);
    if(dill_ismain()) {
        dill_main = ctx;
        rc = atexit(dill_ctx_atexit);
        dill_assert(rc == 0);
    }
    rc = pthread_setspecific(dill_key, ctx);
    dill_assert(rc == 0);
    return ctx;
}

#endif

#endif

