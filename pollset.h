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

/* Initialises the pollset. This function is called once per thread. */
int dill_pollset_init(void);

/* Frees the pollset. This function called once per thread. */
void dill_pollset_term(void);

/* Add waiting for in event on the fd to the list of current clauses. */
int dill_pollset_in(struct dill_clause *cl, int id, int fd);

/* Add waiting for out event on the fd to the list of current clauses. */
int dill_pollset_out(struct dill_clause *cl, int id, int fd);

/* Drops any cached info about the file descriptor. */
void dill_pollset_clean(int fd);

/* Wait for events. 'timeout' is in milliseconds. Returns 0 if timeout was
   exceeded. 1 if at least one clause was triggered. */
int dill_pollset_poll(int timeout);

#endif
