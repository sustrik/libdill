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

#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "../libdill.h"

coroutine void child(int fd) {
    while(1) {
        int rc = yield();
        if(rc < 0 && errno == ECANCELED)
            break;
        assert(rc == 0);
    }
    char c = 55;
    ssize_t sz = write(fd, &c, 1);
    assert(sz == 1);
    close(fd);
}

int main() {
    /* Pipe to check whether child have finished. */
    int fds[2];
    int rc = pipe(fds);
    assert(rc == 0);
    /* Fork an intermediate process. */
    pid_t pid = fork();
    assert(pid >= 0);
    if(pid == 0) {
        /* Intermediate process. */
        int h = proc(child(fds[1]));
        assert(h >= 0);
        rc = msleep(now() + 1000000);
        assert(0);
    }
    /* Parent process. */
    rc = msleep(now() + 200);
    assert(rc == 0);
    rc = kill(pid, SIGKILL);
    assert(rc == 0);
    char c;
    ssize_t sz = read(fds[0], &c, 1);
    assert(sz == 1);
    assert(c == 55);
    close(fds[0]);
    close(fds[1]);
    return 0;
}

