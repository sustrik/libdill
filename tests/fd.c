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

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "assert.h"
#include "../libdill.h"

coroutine void cancel(int fd) {
    int rc = fdin(fd, -1);
    assert(rc == -1 && errno == ECANCELED);
    rc = fdin(fd, -1);
    assert(rc == -1 && errno == ECANCELED);
}

coroutine void trigger(int fd, int64_t deadline) {
    int rc = msleep(deadline);
    errno_assert(rc == 0);
    ssize_t sz = send(fd, "A", 1, 0);
    errno_assert(sz == 1);
}

int main() {
    int rc;

    /* Check invalid fd. */
    rc = fdin(33, -1);
    assert(rc == -1 && errno == EBADF);
    rc = fdout(33, -1);
    assert(rc == -1 && errno == EBADF);

    /* Create a pair of file deshndliptors for testing. */
    int fds[2];
    rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    errno_assert(rc == 0);

    /* Check for in & out. */
    rc = fdout(fds[0], 0);
    errno_assert(rc == 0);
    rc = fdin(fds[0], 0);
    assert(rc == -1 && errno == ETIMEDOUT);

    /* Check with infinite timeout. */
    rc = fdout(fds[0], -1);
    errno_assert(rc == 0);

    /* Check with the timeout that doesn't expire. */
    rc = fdout(fds[0], now() + 100);
    errno_assert(rc == 0);

    /* Check with the timeout that does expire. */
    int64_t deadline = now() + 100;
    rc = fdin(fds[0], deadline);
    assert(rc == -1 && errno == ETIMEDOUT);
    int64_t diff = now() - deadline;
    time_assert(diff, 0);

    /* Check cancelation. */
    int hndl1 = go(cancel(fds[0]));
    errno_assert(hndl1 >= 0);
    rc = hclose(hndl1);
    errno_assert(rc == 0);

    /* Check for in. */
    ssize_t sz = send(fds[1], "A", 1, 0);
    errno_assert(sz == 1);
    rc = fdin(fds[0], -1);
    errno_assert(rc == 0);
    char c;
    sz = recv(fds[0], &c, 1, 0);
    errno_assert(sz == 1);

    /* Two interleaved deadlines. */
    int64_t start = now();
    int hndl2 = go(trigger(fds[0], start + 50));
    errno_assert(hndl2 >= 0);
    rc = fdin(fds[1], start + 1000);
    errno_assert(rc == 0);
    diff = now() - start;
    time_assert(diff, 50);
    rc = hclose(hndl2);
    errno_assert(rc == 0);

    /* Check whether closing the connection is reported. */
    ssize_t nbytes = send(fds[1], "ABC", 3, 0);
    errno_assert(nbytes == 3);
    rc = fdclean(fds[1]);
    errno_assert(rc == 0);
    rc = close(fds[1]);
    errno_assert(rc == 0);
    rc = msleep(now() + 50);
    errno_assert(rc == 0);
    rc = fdin(fds[0], -1);
    errno_assert(rc == 0);
    rc = fdout(fds[0], -1);
    errno_assert(rc == 0);
    char buf[10];
    nbytes = recv(fds[0], buf, sizeof(buf), 0);
    assert(nbytes == 3);
    rc = fdin(fds[0], -1);
    errno_assert(rc == 0);
    nbytes = recv(fds[0], buf, sizeof(buf), 0);
    assert(nbytes == 0 || (nbytes == -1 && errno == ECONNRESET));
    rc = fdclean(fds[0]);
    errno_assert(rc == 0);
    rc = close(fds[0]);
    errno_assert(rc == 0);

    /* Test whether fd is removed from the pollset after fdin/fdout times
       out. */
    int pp[2];
    rc = pipe(pp);
    assert(rc == 0);
    rc = fdin(pp[0], now() + 10);
    assert(rc == -1 && errno == ETIMEDOUT);
    nbytes = write(pp[1], "ABC", 3);
    assert(nbytes == 3);
    rc = msleep(now() + 100);
    assert(rc == 0);
    rc = fdclean(pp[0]);
    errno_assert(rc == 0);
    rc = fdclean(pp[1]);
    errno_assert(rc == 0);
    rc = close(pp[0]);
    assert(rc == 0);
    rc = close(pp[1]);
    assert(rc == 0);

    return 0;
}

