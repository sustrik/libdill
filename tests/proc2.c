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

#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../libdill.h"

int timeout = 0;

coroutine void worker(void) {
    msleep(now() + 100);
    timeout = 1;
}

coroutine void child(int fd) {
    msleep(now() + 200);
    assert(!timeout);
    ssize_t sz = write(fd, "A", 1);
    assert(sz == 1);
    close(fd);
}

int main() {
    /* Pipe to check whether child have failed. */
    int fds[2];
    int rc = pipe(fds);
    assert(rc == 0);
    /* Start second coroutine before forking. */
    int h = go(worker());
    assert(h >= 0);
    /* Fork. */
    h = proc(child(fds[1]));
    assert(h >= 0);
    close(fds[1]);
    /* Parent waits for the child. */
    rc = fdwait(fds[0], FDW_IN, -1);
    assert(rc & FDW_IN);

    return 0;
}

