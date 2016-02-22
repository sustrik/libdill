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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "../libdill.h"

coroutine void cancel(int fd) {
    int rc = fdwait(fd, FDW_IN, -1);
    assert(rc == -1 && errno == ECANCELED);
    rc = fdwait(fd, FDW_IN, -1);
    assert(rc == -1 && errno == ECANCELED);
}

coroutine void trigger(int fd, int64_t deadline) {
    int rc = msleep(deadline);
    assert(rc == 0);
    ssize_t sz = send(fd, "A", 1, 0);
    assert(sz == 1);
}

int main() {
    /* Create a pair of file deshndliptors for testing. */
    int fds[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    assert(rc == 0);

    /* Check for out. */
    rc = fdwait(fds[0], FDW_OUT, -1);
    assert(rc >= 0);
    assert(rc & FDW_OUT);
    assert(!(rc & ~FDW_OUT));

    /* Check with the timeout that doesn't expire. */
    rc = fdwait(fds[0], FDW_OUT, now() + 100);
    assert(rc >= 0);
    assert(rc & FDW_OUT);
    assert(!(rc & ~FDW_OUT));

    /* Check with the timeout that does expire. */
    int64_t deadline = now() + 100;
    rc = fdwait(fds[0], FDW_IN, deadline);
    assert(rc == -1 && errno == ETIMEDOUT);
    int64_t diff = now() - deadline;
    assert(diff > -20 && diff < 20);

    /* Check cancelation. */
    int hndl1 = go(cancel(fds[0]));
    assert(hndl1 >= 0);
    rc = stop(&hndl1, 1, 0);
    assert(rc == 0);

    /* Check for in. */
    ssize_t sz = send(fds[1], "A", 1, 0);
    assert(sz == 1);
    rc = fdwait(fds[0], FDW_IN, -1);
    assert(rc >= 0);
    assert(rc & FDW_IN);
    assert(!(rc & ~FDW_IN));

    /* Check for both in and out. */
    rc = fdwait(fds[0], FDW_IN | FDW_OUT, -1);
    assert(rc >= 0);
    assert(rc & FDW_IN);
    assert(rc & FDW_OUT);
    assert(!(rc & ~(FDW_IN | FDW_OUT)));
    char c;
    sz = recv(fds[0], &c, 1, 0);
    assert(sz == 1);

    /* Two interleaved deadlines. */
    int64_t start = now();
    int hndl2 = go(trigger(fds[0], start + 50));
    assert(hndl2 >= 0);
    rc = fdwait(fds[1], FDW_IN, start + 90);
    assert(rc >= 0);
    assert(rc == FDW_IN);
    diff = now() - start;
    assert(diff > 30 && diff < 70);
    rc = stop(&hndl2, 1, -1);
    assert(rc == 0);

    /* Check whether closing the connection is reported. */
    ssize_t nbytes = send(fds[1], "ABC", 3, 0);
    assert(nbytes == 3);
    fdclean(fds[1]);
    rc = close(fds[1]);
    assert(rc == 0);
    rc = msleep(now() + 50);
    assert(rc == 0);
    rc = fdwait(fds[0], FDW_IN | FDW_OUT, -1);
    assert(rc >= 0);
    assert(rc == FDW_IN | FDW_OUT | FDW_ERR);
    char buf[10];
    nbytes = recv(fds[0], buf, sizeof(buf), 0);
    assert(nbytes == 3);
    rc = fdwait(fds[0], FDW_IN | FDW_OUT, -1);
    assert(rc >= 0);
    assert(rc == FDW_IN | FDW_OUT | FDW_ERR);
    fdclean(fds[0]);
    rc = close(fds[0]);
    assert(rc == 0);

    /* Check whether half-closing the connection is reported. */
    rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    assert(rc == 0);
    rc = shutdown(fds[1], SHUT_WR);
    assert(rc == 0);
    rc = msleep(now() + 50);
    assert(rc == 0);
    rc = fdwait(fds[0], FDW_IN | FDW_OUT, -1);
    assert(rc >= 0);
    assert(rc == FDW_IN | FDW_OUT | FDW_ERR);
    fdclean(fds[0]);
    rc = close(fds[0]);
    assert(rc == 0);
    rc = close(fds[1]);
    assert(rc == 0);

    return 0;
}

