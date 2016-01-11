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

#define TS_CLAUSELEN (sizeof(struct{void *f1; void *f2; void *f3; void *f4; \
    void *f5; void *f6; int f7; int f8; int f9;}))

#define chmake(type, bufsz) ts_chmake(sizeof(type), bufsz,\
    __FILE__ ":" ts_string(__LINE__))

#define chdup(channel) ts_chdup((channel),\
    __FILE__ ":" ts_string(__LINE__))

#define chs(channel, type, value) \
    do {\
        type ts_val = (value);\
        ts_chs((channel), &ts_val, sizeof(type),\
            __FILE__ ":" ts_string(__LINE__));\
    } while(0)

#define chr(channel, type) \
    (*(type*)ts_chr((channel), sizeof(type),\
        __FILE__ ":" ts_string(__LINE__)))

#define chdone(channel, type, value) \
    do {\
        type ts_val = (value);\
        ts_chdone((channel), &ts_val, sizeof(type),\
             __FILE__ ":" ts_string(__LINE__));\
    } while(0)

#define chclose(channel) ts_chclose((channel),\
    __FILE__ ":" ts_string(__LINE__))

TS_EXPORT chan ts_chmake(size_t sz, size_t bufsz, const char *created);
TS_EXPORT chan ts_chdup(chan ch, const char *created);
TS_EXPORT void ts_chs(chan ch, void *val, size_t sz, const char *current);
TS_EXPORT void *ts_chr(chan ch, size_t sz, const char *current);
TS_EXPORT void ts_chdone(chan ch, void *val, size_t sz,
    const char *current);
TS_EXPORT void ts_chclose(chan ch, const char *current);

#define ts_concat(x,y) x##y

#define choose \
    {\
        ts_choose_init(__FILE__ ":" ts_string(__LINE__));\
        int ts_idx = -2;\
        while(1) {\
            if(ts_idx != -2) {\
                if(0)

#define ts_in(chan, type, name, idx) \
                    break;\
                }\
                goto ts_concat(ts_label, idx);\
            }\
            char ts_concat(ts_clause, idx)[TS_CLAUSELEN];\
            ts_choose_in(\
                &ts_concat(ts_clause, idx)[0],\
                (chan),\
                sizeof(type),\
                idx);\
            if(0) {\
                type name;\
                ts_concat(ts_label, idx):\
                if(ts_idx == idx) {\
                    name = *(type*)ts_choose_val(sizeof(type));\
                    goto ts_concat(ts_dummylabel, idx);\
                    ts_concat(ts_dummylabel, idx)

#define in(chan, type, name) ts_in((chan), type, name, __COUNTER__)

#define ts_out(chan, type, val, idx) \
                    break;\
                }\
                goto ts_concat(ts_label, idx);\
            }\
            char ts_concat(ts_clause, idx)[TS_CLAUSELEN];\
            type ts_concat(ts_val, idx) = (val);\
            ts_choose_out(\
                &ts_concat(ts_clause, idx)[0],\
                (chan),\
                &ts_concat(ts_val, idx),\
                sizeof(type),\
                idx);\
            if(0) {\
                ts_concat(ts_label, idx):\
                if(ts_idx == idx) {\
                    goto ts_concat(ts_dummylabel, idx);\
                    ts_concat(ts_dummylabel, idx)

#define out(chan, type, val) ts_out((chan), type, (val), __COUNTER__)

#define ts_deadline(ddline, idx) \
                    break;\
                }\
                goto ts_concat(ts_label, idx);\
            }\
            ts_choose_deadline(ddline);\
            if(0) {\
                ts_concat(ts_label, idx):\
                if(ts_idx == -1) {\
                    goto ts_concat(ts_dummylabel, idx);\
                    ts_concat(ts_dummylabel, idx)

#define deadline(ddline) ts_deadline(ddline, __COUNTER__)

#define ts_otherwise(idx) \
                    break;\
                }\
                goto ts_concat(ts_label, idx);\
            }\
            ts_choose_otherwise();\
            if(0) {\
                ts_concat(ts_label, idx):\
                if(ts_idx == -1) {\
                    goto ts_concat(ts_dummylabel, idx);\
                    ts_concat(ts_dummylabel, idx)

#define otherwise ts_otherwise(__COUNTER__)

#define end \
                    break;\
                }\
            }\
            ts_idx = ts_choose_wait();\
        }

TS_EXPORT void ts_choose_init(const char *current);
TS_EXPORT void ts_choose_in(void *clause, chan ch, size_t sz, int idx);
TS_EXPORT void ts_choose_out(void *clause, chan ch, void *val, size_t sz,
    int idx);
TS_EXPORT void ts_choose_deadline(int64_t ddline);
TS_EXPORT void ts_choose_otherwise(void);
TS_EXPORT int ts_choose_wait(void);
TS_EXPORT void *ts_choose_val(size_t sz);

/******************************************************************************/
/*  Debugging                                                                 */
/******************************************************************************/

TS_EXPORT void goredump(void);
TS_EXPORT void gotrace(int level);

#endif

