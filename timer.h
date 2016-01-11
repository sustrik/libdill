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

#ifndef TS_TIMER_INCLUDED
#define TS_TIMER_INCLUDED

#include <stdint.h>

#include "list.h"

struct ts_timer;

typedef void (*ts_timer_callback)(struct ts_timer *timer);

struct ts_timer {
    /* Item in the global list of all timers. */
    struct ts_list_item item;
    /* The deadline when the timer expires. */
    int64_t expiry;
    /* Callback invoked when timer expires. Pfui Teufel! */
    ts_timer_callback callback;
};

/* Add a timer for the running coroutine. */
void ts_timer_add(struct ts_timer *timer, int64_t deadline,
    ts_timer_callback callback);

/* Remove the timer associated with the running coroutine. */
void ts_timer_rm(struct ts_timer *timer);

/* Number of milliseconds till the next timer expires.
   If there are no timers returns -1. */
int ts_timer_next(void);

/* Resumes all coroutines whose timers have already expired.
   Returns zero if no coroutine was resumed, 1 otherwise. */
int ts_timer_fire(void);

#endif

