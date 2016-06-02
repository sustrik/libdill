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

#ifndef LIBDILL_H_INCLUDED
#define LIBDILL_H_INCLUDED

#include <errno.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

/******************************************************************************/
/*  ABI versioning support                                                    */
/******************************************************************************/

/*  Don't change this unless you know exactly what you're doing and have      */
/*  read and understand the following documents:                              */
/*  www.gnu.org/software/libtool/manual/html_node/Libtool-versioning.html     */
/*  www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html  */

/*  The current interface version. */
#define DILL_VERSION_CURRENT 3

/*  The latest revision of the current interface. */
#define DILL_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define DILL_VERSION_AGE 1

/******************************************************************************/
/*  Symbol visibility                                                         */
/******************************************************************************/

#if !defined __GNUC__ && !defined __clang__
#error "Unsupported compiler!"
#endif

#if DILL_NO_EXPORTS
#define DILL_EXPORT
#else
#define DILL_EXPORT __attribute__ ((visibility("default")))
#endif

/* Old versions of GCC don't support visibility attribute. */
#if defined __GNUC__ && __GNUC__ < 4
#undef DILL_EXPORT
#define DILL_EXPORT
#endif

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

DILL_EXPORT int64_t now(void);

/******************************************************************************/
/*  Handles                                                                   */
/******************************************************************************/

struct hvfptrs {
    int (*finish)(int h, int64_t deadline);
    void (*close)(int h);
};

DILL_EXPORT int handle(const void *type, void *data,
    const struct hvfptrs *vfptrs);
DILL_EXPORT int hdup(int h);
DILL_EXPORT void *hdata(int h, const void *type);
DILL_EXPORT int hfinish(int h, int64_t deadline);
DILL_EXPORT int hclose(int h);

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

#define coroutine __attribute__((noinline))

DILL_EXPORT extern volatile int dill_unoptimisable1;
DILL_EXPORT extern volatile void *dill_unoptimisable2;

DILL_EXPORT __attribute__((noinline)) int dill_prologue(sigjmp_buf **ctx);
DILL_EXPORT __attribute__((noinline)) void dill_epilogue(void);
DILL_EXPORT int dill_proc_prologue(int *hndl);
DILL_EXPORT void dill_proc_epilogue(void);

/* Statement expressions are a gcc-ism but they are also supported by clang.
   Given that there's no other way to do this, screw other compilers for now.
   See https://gcc.gnu.org/onlinedocs/gcc-3.2/gcc/Statement-Exprs.html */
#define go(fn) \
    ({\
        sigjmp_buf *ctx;\
        int h = dill_prologue(&ctx);\
        if(h >= 0) {\
            if(!sigsetjmp(*ctx, 0)) {\
                int dill_anchor[dill_unoptimisable1];\
                dill_unoptimisable2 = &dill_anchor;\
                char dill_filler[(char*)&dill_anchor - (char*)hdata(h, NULL)];\
                dill_unoptimisable2 = &dill_filler;\
                fn;\
                dill_epilogue();\
            }\
        }\
        h;\
    })

#define proc(fn) \
    ({\
        int h;\
        if(dill_proc_prologue(&h)) {\
            fn;\
            dill_proc_epilogue();\
        }\
        h;\
    })

DILL_EXPORT int yield(void);
DILL_EXPORT int msleep(int64_t deadline);
DILL_EXPORT void fdclean(int fd);
DILL_EXPORT int fdin(int fd, int64_t deadline);
DILL_EXPORT int fdout(int fd, int64_t deadline);
DILL_EXPORT void *cls(void);
DILL_EXPORT void setcls(void *val);

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

#define CHSEND 1
#define CHRECV 2

struct chclause {
    int op;
    int ch;
    void *val;
    size_t len;
    char reserved[64];
};

DILL_EXPORT int channel(size_t itemsz, size_t bufsz);
DILL_EXPORT int chsend(int ch, const void *val, size_t len, int64_t deadline);
DILL_EXPORT int chrecv(int ch, void *val, size_t len, int64_t deadline);
DILL_EXPORT int chdone(int ch);
DILL_EXPORT int choose(struct chclause *clauses, int nclauses,
    int64_t deadline);

#endif

