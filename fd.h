/*

  Copyright (c) 2019 Martin Sustrik

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

#ifndef DILL_FD_INCLUDED
#define DILL_FD_INCLUDED

#include <sys/socket.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdill.h"

/* Given a non-blocking listener socket this function accepts a new connection
   in such a way that it yields to other coroutines while blocking. */
int dill_fd_accept(
    int s,
    struct sockaddr *addr,
    socklen_t *addrlen,
    int64_t deadline);

/* Check whether s has particular type (e.g. SOCK_STREAM vs. SOCK_DGRAM),
   address family and whether it is a listening socket. Returns 1 if the socket
   matches, zero otherwise. */
int dill_fd_check(
    int s,
    int type,
    int family1,
    int family2,
    int listener);

/* Close a file descriptor. This fuction also calls fdclean(). */
void dill_fd_close(
    int s);

/* Establish a connection while yeilding the control to other coroutines
   if it can't be done immediately. */
int dill_fd_connect(
    int s,
    const struct sockaddr *addr,
    socklen_t addrlen,
    int64_t deadline);

/* Transfer the ownership of the file descriptor to the owner. This is
   accomplished by closing the file descriptor and returning a different
   one pointing to the same underlying object. */
int dill_fd_own(
    int s);

/* Tune the socket to be usable by libdill. This means putting it into
   non-blocking mode, disabling SIGPIPE signals and similar. */
int dill_fd_tune(
    int s);

#endif


