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

#ifndef DILL_POLLSET_INCLUDED
#define DILL_POLLSET_INCLUDED

/* User overloads. */
#if defined DILL_EPOLL
#include "epoll.h.inc"
#elif defined DILL_KQUEUE
#include "kqueue.h.inc"
#elif defined DILL_POLL
#include "poll.h.inc"
/* Defaults. */
#elif defined HAVE_EPOLL
#include "epoll.h.inc"
#elif defined HAVE_KQUEUE
#include "kqueue.h.inc"
#else
#include "poll.h.inc"
#endif

int dill_ctx_pollset_init(struct dill_ctx_pollset *ctx);
void dill_ctx_pollset_term(struct dill_ctx_pollset *ctx);

/* Add waiting for an in event on the fd to the list of current clauses. */
int dill_pollset_in(struct dill_fdclause *fdcl, int id, int fd);

/* Add waiting for an out event on the fd to the list of current clauses. */
int dill_pollset_out(struct dill_fdclause *fdcl, int id, int fd);

/* Drop any cached info about the file descriptor. */
int dill_pollset_clean(int fd);

/* Wait for events. 'timeout' is in milliseconds. Return 0 if the timeout expired or
  1 if at least one clause was triggered. */
int dill_pollset_poll(int timeout);

#endif

