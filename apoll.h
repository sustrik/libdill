/*

  Copyright (c) 2017 Martin Sustrik

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

#ifndef DILL_APOLL_INCLUDED
#define DILL_APOLL_INCLUDED

/* Definition of struct dill_apoll is platform-specific. */

/* User overloads. */
#if defined DILL_EPOLL
#include "apoll-epoll.h.inc"
#elif defined DILL_KQUEUE
#include "apoll-kqueue.h.inc"
#elif defined DILL_POLL
#include "apoll-poll.h.inc"
/* Defaults. */
#elif defined HAVE_EPOLL
#include "apoll-epoll.h.inc"
#elif defined HAVE_KQUEUE
#include "apoll-kqueue.h.inc"
#else
#include "apoll-poll.h.inc"
#endif

/* apoll is an abstraction designed to provide unified API for different
   polling mechanisms, such as select, poll, kqueue, epoll or event ports.
   It's not clear whether IOCP can be shoehorned into this model, but luckily,
   we don't support Windows. */

#define DILL_IN 1
#define DILL_OUT 2

struct dill_apoll;

int dill_apoll_init(struct dill_apoll *self);
void dill_apoll_term(struct dill_apoll *self);

/* The API works in three phases:

   1. Adjust the pollset using dill_apoll_ctl.
   2. Do the poll using dill_apoll_poll.
   3. Retrieve all the events using dill_apoll_event.
   4. Repeat.

   Using it in any other way results in undefined behaviour.

   This way we can ignore pathologic cases like modifying the pollset while
   retrieving the results from previous poll.
*/

/* Events is a combination of DILL_IN and DILL_OUT. 'old' is a hint about
   currently active triggers. Implementation may or may not use the value. */
void dill_apoll_ctl(struct dill_apoll *self, int fd, int events, int old);

/* Waits for events. Timeout is in milliseconds. */
int dill_apoll_wait(struct dill_apoll *self, int timeout);

/* If both IN and OUT are triggered on the same file descriptor, the function
   is free to return them as two separate events or as a single event with
   events argument set to DILL_NPOLL_IN | DILL_NPOLL_OUT. The implementation
   must not return the same event twice without intermittent call to
   dill_apoll_poll. Returns 0 in case of success, -1 if there are no more
   events to report. */
int dill_apoll_event(struct dill_apoll *self, int *fd, int *events);

#endif

