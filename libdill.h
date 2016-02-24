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
#define DILL_VERSION_CURRENT 2

/*  The latest revision of the current interface. */
#define DILL_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define DILL_VERSION_AGE 0

/******************************************************************************/
/*  Symbol visibility                                                         */
/******************************************************************************/

#if defined DILL_NO_EXPORTS
#   define DILL_EXPORT
#else
#   if defined _WIN32
#      if defined DILL_EXPORTS
#          define DILL_EXPORT __declspec(dllexport)
#      else
#          define DILL_EXPORT __declspec(dllimport)
#      endif
#   else
#      if defined __SUNPRO_C
#          define DILL_EXPORT __global
#      elif (defined __GNUC__ && __GNUC__ >= 4) || \
             defined __INTEL_COMPILER || defined __clang__
#          define DILL_EXPORT __attribute__ ((visibility("default")))
#      else
#          define DILL_EXPORT
#      endif
#   endif
#endif

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

DILL_EXPORT int64_t now(void);

/******************************************************************************/
/*  Handles                                                                   */
/******************************************************************************/

typedef void (*hndlstop_fn)(int h);

DILL_EXPORT int handle(const void *type, void *data, hndlstop_fn stop_fn);
DILL_EXPORT const void *handletype(int h);
DILL_EXPORT void *handledata(int h);
DILL_EXPORT int handledone(int h, int result);

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

DILL_EXPORT extern volatile int dill_unoptimisable1;
DILL_EXPORT extern volatile void *dill_unoptimisable2;

DILL_EXPORT __attribute__((noinline)) int dill_prologue(int *hndl,
    const char *created);
DILL_EXPORT __attribute__((noinline)) void dill_epilogue(int result);

#define dill_string2(x) #x
#define dill_string(x) dill_string2(x)

#if defined __GNUC__ || defined __clang__
#define coroutine __attribute__((noinline))
#else
#error "Unsupported compiler!"
#endif

/* Statement expressions are a gcc-ism but they are also supported by clang.
   Given that there's no other way to do this, screw other compilers for now.
   See https://gcc.gnu.org/onlinedocs/gcc-3.2/gcc/Statement-Exprs.html */
#define go(fn) \
    ({\
        int hndl;\
        if(dill_prologue(&hndl, __FILE__ ":" dill_string(__LINE__))) {\
            int dill_anchor[dill_unoptimisable1];\
            dill_unoptimisable2 = &dill_anchor;\
            char dill_filler[(char*)&dill_anchor - (char*)handledata(hndl)];\
            dill_unoptimisable2 = &dill_filler;\
            dill_epilogue(fn);\
        }\
        hndl;\
    })

#define FDW_IN 1
#define FDW_OUT 2
#define FDW_ERR 4

#define yield() dill_yield(__FILE__ ":" dill_string(__LINE__))
#define stop(crs, ncrs, deadline) dill_stop((crs), (ncrs), (deadline), \
    __FILE__ ":" dill_string(__LINE__))
#define msleep(deadline) dill_msleep((deadline),\
    __FILE__ ":" dill_string(__LINE__))
#define fdwait(fd, events, deadline) dill_fdwait((fd), (events), (deadline),\
    __FILE__ ":" dill_string(__LINE__))

DILL_EXPORT int dill_yield(const char *current);
DILL_EXPORT int dill_stop(int *hndls, int nhndls, int64_t deadline,
    const char *current);
DILL_EXPORT int dill_msleep(int64_t deadline, const char *current);
DILL_EXPORT void fdclean(int fd);
DILL_EXPORT int dill_fdwait(int fd, int events, int64_t deadline,
    const char *current);
DILL_EXPORT pid_t mfork(void);
DILL_EXPORT void *cls(void);
DILL_EXPORT void setcls(void *val);

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

typedef struct dill_chan *chan;

#define CHSEND 1
#define CHRECV 2

struct chclause {
    chan channel;
    int op;
    void *val;
    size_t len;
    void *reserved1;
    void *reserved2;
    void *reserved3;
    int reserved4;
    int reserved5;
};

#define channel(itemsz, bufsz) \
    dill_channel((itemsz), (bufsz), __FILE__ ":" dill_string(__LINE__))

#define chdup(channel) \
   dill_chdup((channel), __FILE__ ":" dill_string(__LINE__))

#define chsend(channel, val, len, deadline) \
    dill_chsend((channel), (val), (len), (deadline), \
    __FILE__ ":" dill_string(__LINE__))

#define chrecv(channel, val, len, deadline) \
    dill_chrecv((channel), (val), (len), (deadline), \
    __FILE__ ":" dill_string(__LINE__))

#define chdone(channel) \
    dill_chdone((channel), __FILE__ ":" dill_string(__LINE__))

#define chclose(channel) \
    dill_chclose((channel), __FILE__ ":" dill_string(__LINE__))

#define choose(clauses, nclauses, deadline) \
    dill_choose((clauses), (nclauses), (deadline), \
    __FILE__ ":" dill_string(__LINE__))

DILL_EXPORT chan dill_channel(size_t itemsz, size_t bufsz,
    const char *created);
DILL_EXPORT chan dill_chdup(chan ch,
    const char *created);
DILL_EXPORT int dill_chsend(chan ch, const void *val, size_t len,
    int64_t deadline, const char *current);
DILL_EXPORT int dill_chrecv(chan ch, void *val, size_t len,
    int64_t deadline, const char *current);
DILL_EXPORT int dill_chdone(chan ch, const char *current);
DILL_EXPORT void dill_chclose(chan ch,
    const char *current);
DILL_EXPORT int dill_choose(struct chclause *clauses, int nclauses,
    int64_t deadline, const char *current);

/******************************************************************************/
/*  Debugging                                                                 */
/******************************************************************************/

DILL_EXPORT void goredump(void);
DILL_EXPORT void gotrace(int level);

#endif

