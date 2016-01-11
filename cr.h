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

#ifndef TS_CR_INCLUDED
#define TS_CR_INCLUDED

#include <stdint.h>

#include "chan.h"
#include "debug.h"
#include "list.h"
#include "slist.h"
#include "timer.h"
#include "utils.h"

enum ts_state {
    TS_READY,
    TS_MSLEEP,
    TS_FDWAIT,
    TS_CHR,
    TS_CHS,
    TS_CHOOSE
};

/* The coroutine. The memory layout looks like this:

   +----------------------------------------------------+--------+---------+
   |                                              stack | valbuf | ts_cr |
   +----------------------------------------------------+--------+---------+

   - ts_cr contains generic book-keeping info about the coroutine
   - valbuf is a buffer for temporarily storing values received from channels
   - stack is a standard C stack; it grows downwards (at the moment libmill
     doesn't support microarchitectures where stack grows upwards)

*/
struct ts_cr {
    /* Status of the coroutine. Used for debugging purposes. */
    enum ts_state state;

    /* The coroutine is stored in this list if it is not blocked and it is
       waiting to be executed. */
    struct ts_slist_item ready;

    /* If the coroutine is waiting for a deadline, it uses this timer. */
    struct ts_timer timer;

    /* When the coroutine is blocked in fdwait(), these members contains the
       file descriptor and events that the function waits for. They are used
       only for debugging purposes. */
    int fd;
    int events;

    /* This structure is used when the coroutine is executing a choose
       statement. */
    struct ts_choosedata choosedata;

    /* Stored coroutine context while it is not executing. */
    struct ts_ctx ctx;

    /* Argument to resume() call being passed to the blocked suspend() call. */
    int result;

    /* If size of the valbuf needs to be larger than ts_valbuf size it is
       allocated dyncamically and the pointer, along with the size of the buffer
       is stored here. */
    void *valbuf;
    size_t valbuf_sz;

    /* Coroutine-local storage. */
    void *cls;

    /* Debugging info. */
    struct ts_debug_cr debug;
};

/* Fake coroutine corresponding to the main thread of execution. */
extern struct ts_cr ts_main;

/* The coroutine that is running at the moment. */
extern struct ts_cr *ts_running;

/* Suspend running coroutine. Move to executing different coroutines. Once
   someone resumes this coroutine using ts_resume function 'result' argument
   of that function will be returned. */
int ts_suspend(void);

/* Schedules preiously suspended coroutine for execution. Keep in mind that
   it doesn't immediately run it, just puts it into the queue of ready
   coroutines. */
void ts_resume(struct ts_cr *cr, int result);

/* Returns pointer to the value buffer. The returned buffer is guaranteed
   to be at least 'size' bytes long. */
void *ts_valbuf(struct ts_cr *cr, size_t size);

#endif
