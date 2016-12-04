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

#include <stdlib.h>
#include <unistd.h>

#include "libdill.h"
#include "ctx.h"
#include "utils.h"

#if defined(DILL_THREADS) && defined(DILL_SHARED)
DILL_THREAD_LOCAL struct dill_ctx dill_context = {0};
#endif

#if defined DILL_THREADS
/* Needed to determine if current thread is the main thread on Linux. */
#if defined __linux__
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#endif

/* Needed to determine if current thread is the main thread on NetBSD. */
#if defined __NetBSD__
#include <sys/lwp.h>
#endif

/* Needed to determine if current thread is the main thread on other Unix. */
#if defined(__OpenBSD__) || defined(__FreeBSD__) ||\
    defined(__APPLE__) || defined(__DragonFly__) || defined(__sun)
#include <pthread.h>
#endif

/* Returns 1 if in main thread, otherwise 0 for spawned threads. */
static int dill_ismain() {
#if defined __linux__
    return syscall(SYS_gettid) == getpid();
#elif defined(__OpenBSD__) || defined(__FreeBSD__) ||\
    defined(__APPLE__) || defined(__DragonFly__)
    return pthread_main_np();
#elif defined __NetBSD__
    return _lwp_self() == 1;
#elif defined __sun
    return pthread_self() == 1;
#else
    return -1;
#endif
}
#else
/* Single-threaded case always returns 1. */
static int dill_ismain() {return 1;}
#endif

#if !(defined(DILL_THREADS) && defined(DILL_SHARED))
static DILL_THREAD_LOCAL struct dill_atexit_fn_list dill_fn_list = {0};
#endif

/* Flag to indicate whether the atexit has been registered. */
static int dill_main_atexit_initialized = 0;

/* Run registered destructor functions on thread exit. */
static void dill_destructor(void *ptr) {
    struct dill_atexit_fn_list *list = ptr;
    dill_assert(list != NULL);
    int i = list->count - 1;
    for(; i >= 0; --i) {
        dill_assert(list->pair[i].fn != NULL);
        dill_assert(list->pair[i].ptr != NULL);
        list->pair[i].fn(list->pair[i].ptr);
    }
}

/* Register a destructor function. */
static int dill_atexit_register(struct dill_atexit_fn_list *list,
        void *fn, void *ptr) {
    if(list->count >= DILL_ATEXIT_MAX) {errno = ENOMEM; return -1;}
    if(fn == NULL || ptr == NULL) {errno = EINVAL; return -1;}
    list->pair[list->count].fn = fn;
    list->pair[list->count].ptr = ptr;
    list->count++;
    return 0;
}

/* Get the list context */
static inline struct dill_atexit_fn_list *dill_atexit_list(void) {
#if defined(DILL_THREADS) && defined(DILL_SHARED)
    return &dill_context.fn_list;
#else
    return &dill_fn_list;
#endif
}

/* Atexit handler for main function. */
static void dill_atexit_handler(void) {
    dill_destructor(dill_atexit_list());
}

#if defined(DILL_THREADS)
#if defined(DILL_PTHREAD)
#include <pthread.h>
static pthread_key_t dill_key;
static pthread_once_t dill_keyonce = PTHREAD_ONCE_INIT;
static void dill_makekey(void) {
    int rc = pthread_key_create(&dill_key, dill_destructor);
    dill_assert(!rc);
}
#else
#error "No thread destructor support"
#endif
#endif

/* Runs atexit on main thread of simulates atexit on spawned threads. */
int dill_atexit(dill_atexit_fn fn, void *ptr) {
    struct dill_atexit_fn_list *list = dill_atexit_list();
    int rc = dill_ismain();
    if(rc == 1) {
        if(dill_slow(!dill_main_atexit_initialized)) {
            rc = atexit(dill_atexit_handler);
            if(rc != 0) return -1;
            dill_main_atexit_initialized = 1;
        }
    }
#if defined DILL_THREADS
#if defined DILL_PTHREAD
    /* Create pthread key destructor. */
    if (rc == 0) {
        rc = pthread_once(&dill_keyonce, dill_makekey);
        if(rc != 0) {errno = rc; return -1;}
        void *ptr = pthread_getspecific(dill_key);
        if(dill_slow(!ptr)) {
            rc = pthread_setspecific(dill_key, list);
            if(rc != 0) {errno = rc; return -1;}
        }
    }
#else
#error "No thread destructor support"
#endif
#endif
    return dill_atexit_register(list, fn, ptr);
}

