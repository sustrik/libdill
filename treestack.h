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

#ifndef TREESTACK_H_INCLUDED
#define TREESTACK_H_INCLUDED

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
#define TS_VERSION_CURRENT 0

/*  The latest revision of the current interface. */
#define TS_VERSION_REVISION 0

/*  How many past interface versions are still supported. */
#define TS_VERSION_AGE 0

/******************************************************************************/
/*  Symbol visibility                                                         */
/******************************************************************************/

#if defined TS_NO_EXPORTS
#   define TS_EXPORT
#else
#   if defined _WIN32
#      if defined TS_EXPORTS
#          define TS_EXPORT __declspec(dllexport)
#      else
#          define TS_EXPORT __declspec(dllimport)
#      endif
#   else
#      if defined __SUNPRO_C
#          define TS_EXPORT __global
#      elif (defined __GNUC__ && __GNUC__ >= 4) || \
             defined __INTEL_COMPILER || defined __clang__
#          define TS_EXPORT __attribute__ ((visibility("default")))
#      else
#          define TS_EXPORT
#      endif
#   endif
#endif

/******************************************************************************/
/*  Helpers                                                                   */
/******************************************************************************/

TS_EXPORT int64_t now(void);

/******************************************************************************/
/*  Coroutines                                                                */
/******************************************************************************/

TS_EXPORT extern volatile int ts_unoptimisable1;
TS_EXPORT extern volatile void *ts_unoptimisable2;

TS_EXPORT void *ts_prologue(const char *created);
TS_EXPORT void ts_epilogue(void);

#define ts_string2(x) #x
#define ts_string(x) ts_string2(x)

#if defined __GNUC__ || defined __clang__
#define coroutine __attribute__((noinline))
#else
#define coroutine
#endif

#define go(fn) \
    do {\
        void *ts_sp = ts_prologue(__FILE__ ":" ts_string(__LINE__));\
        if(ts_sp) {\
            int ts_anchor[ts_unoptimisable1];\
            ts_unoptimisable2 = &ts_anchor;\
            char ts_filler[(char*)&ts_anchor - (char*)(ts_sp)];\
            ts_unoptimisable2 = &ts_filler;\
            fn;\
            ts_epilogue();\
        }\
    } while(0)

#define yield() ts_yield(__FILE__ ":" ts_string(__LINE__))

TS_EXPORT int ts_yield(const char *current);

#define msleep(deadline) ts_msleep((deadline),\
    __FILE__ ":" ts_string(__LINE__))

TS_EXPORT int ts_msleep(int64_t deadline, const char *current);

#define fdwait(fd, events, deadline) ts_fdwait((fd), (events), (deadline),\
    __FILE__ ":" ts_string(__LINE__))

TS_EXPORT void fdclean(int fd);

#define FDW_IN 1
#define FDW_OUT 2
#define FDW_ERR 4

TS_EXPORT int ts_fdwait(int fd, int events, int64_t deadline,
    const char *current);

TS_EXPORT pid_t mfork(void);

TS_EXPORT void *cls(void);
TS_EXPORT void setcls(void *val);

/******************************************************************************/
/*  Channels                                                                  */
/******************************************************************************/

typedef struct ts_chan *chan;

#define CHOOSE_CHS 1
#define CHOOSE_CHR 2

struct chclause {
    chan channel;
    int op;
    void *val;
    size_t len;
    void *reserved1;
    void *reserved2;
    void *reserved3;
    void *reserved4;
    void *reserved5;
    int reserved6;
    int reserved7;
    int reserved8;
};

#define chmake(itemsz, bufsz) \
    ts_chmake((itemsz), (bufsz), __FILE__ ":" ts_string(__LINE__))

#define chdup(channel) \
   ts_chdup((channel), __FILE__ ":" ts_string(__LINE__))

#define chs(channel, val, len) \
    ts_chs((channel), (val), (len), __FILE__ ":" ts_string(__LINE__))

#define chr(channel, val, len) \
    ts_chr((channel), (val), (len), __FILE__ ":" ts_string(__LINE__))

#define chdone(channel, val, len) \
    ts_chdone((channel), (val), (len), __FILE__ ":" ts_string(__LINE__))

#define chclose(channel) \
    ts_chclose((channel), __FILE__ ":" ts_string(__LINE__))

#define choose(clauses, nclauses, deadline) \
    ts_choose((clauses), (nclauses), (deadline), \
    __FILE__ ":" ts_string(__LINE__))

TS_EXPORT chan ts_chmake(size_t itemsz, size_t bufsz, const char *created);
TS_EXPORT chan ts_chdup(chan ch, const char *created);
TS_EXPORT int ts_chs(chan ch, const void *val, size_t len, const char *current);
TS_EXPORT int ts_chr(chan ch, void *val, size_t len, const char *current);
TS_EXPORT int ts_chdone(chan ch, const void *val, size_t len,
    const char *current);
TS_EXPORT void ts_chclose(chan ch, const char *current);
TS_EXPORT int ts_choose(struct chclause *clauses, int nclauses,
    int64_t deadline, const char *current);

/******************************************************************************/
/*  Debugging                                                                 */
/******************************************************************************/

TS_EXPORT void goredump(void);
TS_EXPORT void gotrace(int level);

#endif

