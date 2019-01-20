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

#include <fcntl.h>
#include <unistd.h>

#include "fd.h"
#include "utils.h"

int dill_fd_accept(int s, struct sockaddr *addr, socklen_t *addrlen,
      int64_t deadline) {
    while(1) {
        /* Try to accept new connection synchronously. */
        int as = accept(s, addr, addrlen);
        if(dill_fast(as >= 0)) return as;
        /* If connection was aborted by the peer grab the next one. */
        if(dill_slow(errno == ECONNABORTED)) continue;
        /* TODO: Shoul this nullify the socket? */
        if(dill_slow(errno != EAGAIN && errno != EWOULDBLOCK)) return -1;
        /* Operation is in progress. Wait till new connection is available. */
        int rc = dill_fdin(s, deadline);
        if(dill_slow(rc < 0)) return -1;
    }
}

int dill_fd_check(int s, int type, int family1, int family2, int listener) {
    int val;
    socklen_t valsz = sizeof(val);
    int rc = getsockopt(s, SOL_SOCKET, SO_TYPE, &val, &valsz);
    if(dill_slow(rc < 0)) return -1;
    if(dill_slow(val != type)) return 0;

    struct sockaddr_storage ss;
    socklen_t sssz = sizeof(ss);
    rc = getsockname(s, (struct sockaddr*)&ss, &sssz);
    if(dill_slow(rc < 0)) return -1;
    if(dill_slow(ss.ss_family != family1 && ss.ss_family != family2)) return 0;

    rc = getsockopt(s, SOL_SOCKET, SO_ACCEPTCONN, &val, &valsz);
    /* This call returns ENOPROTOOPT on OSX. It sucks but if an OS doesn't
       implement POSIX it's pretty hard to care. We just skip the check in
       such case. */
    if(dill_slow(rc < 0 && errno == ENOPROTOOPT)) return 1;
    if(dill_slow(rc < 0)) return -1;
    if(listener == 0 && val != 0) return 0;
    if(listener != 0 && val == 0) return 0;

    return 1;
}

void dill_fd_close(int s) {
    int rc = dill_fdclean(s);
    dill_errno_assert(rc == 0);
    /* Discard any pending outbound data. If SO_LINGER option cannot
       be set, never mind and continue anyway. */
    struct linger lng;
    lng.l_onoff=1;
    lng.l_linger=0;
    setsockopt(s, SOL_SOCKET, SO_LINGER, (void*)&lng, sizeof(lng));
    /* We are not checking the error here. close() has inconsistent behaviour.
       For example, if the call failes with EINTR on HP-UX the socket it not
       closed. However, it's closed in almost all cases and even if it's not
       leaking a file descriptor is better than crashing the entire program. */
    close(s);
}

int dill_fd_connect(int s, const struct sockaddr *addr, socklen_t addrlen,
      int64_t deadline) {
    /* Initiate connect. */
    int rc = connect(s, addr, addrlen);
    if(rc == 0) return 0;
    if(dill_slow(errno != EINPROGRESS)) return -1;
    /* Connect is in progress. Let's wait till it's done. */
    rc = dill_fdout(s, deadline);
    if(dill_slow(rc == -1)) return -1;
    /* Retrieve the error from the socket, if any. */
    int err = 0;
    socklen_t errsz = sizeof(err);
    rc = getsockopt(s, SOL_SOCKET, SO_ERROR, (void*)&err, &errsz);
    if(dill_slow(rc != 0)) return -1;
    if(dill_slow(err != 0)) {errno = err; return -1;}
    return 0;
}

int dill_fd_own(int s) {
    int n = dup(s);
    if(dill_slow(n < 0)) return -1;
    dill_fd_close(s);
    return n;
}

int dill_fd_tune(int s) {
    /* Switch to non-blocking mode. */
    int opt = fcntl(s, F_GETFL, 0);
    if (opt == -1)
        opt = 0;
    int rc = fcntl(s, F_SETFL, opt | O_NONBLOCK);
    dill_errno_assert(rc == 0);
    /*  Allow re-using the same local address rapidly. */
    /*  TODO: Should this be an option?
    opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));
    dill_errno_assert(rc == 0);
    /* If possible, prevent SIGPIPE signal when writing to the connection
        already closed by the peer. */
#ifdef SO_NOSIGPIPE
    opt = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &opt, sizeof (opt));
    dill_errno_assert (rc == 0 || errno == EINVAL);
#endif
    return 0;
}

