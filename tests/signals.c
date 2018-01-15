/*

  Copyright (c) 2015 Nir Soffer

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

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "assert.h"
#include "../libdill.h"

#define SIGNAL SIGUSR1
#define COUNT 1000

static int signal_pipe[2];

void signal_intr(int signo) {
    assert(signo == SIGNAL);
    char b = signo;
    ssize_t sz = write(signal_pipe[1], &b, 1);
    assert(sz == 1);
}

coroutine void sender(int ch) {
    char signo;
    int rc = chrecv(ch, &signo, sizeof(signo), -1);
    errno_assert(rc == 0);
    rc = kill(getpid(), signo);
    errno_assert(rc == 0);
}

coroutine void receiver(int ch) {
    int events = fdin(signal_pipe[0], -1);
    errno_assert(events == 0);

    char signo;
    ssize_t sz = read(signal_pipe[0], &signo, 1);
    assert(sz == 1);
    assert(signo == SIGNAL);

    int rc = chsend(ch, &signo, sizeof(signo), -1);
    errno_assert(rc == 0);
}

int main() {
    int err = pipe(signal_pipe);
    errno_assert(err == 0);

    signal(SIGNAL, signal_intr);

    int sendch[2];
    int rc = chmake(sendch);
    errno_assert(rc == 0);
    int recvch[2];
    rc = chmake(recvch);
    errno_assert(rc == 0);

    int i;
    for(i = 0; i < COUNT; ++i) {
        int hndls[2];
        hndls[0] = go(sender(sendch[0]));
        errno_assert(hndls[0] >= 0);
        hndls[1] = go(receiver(recvch[0]));
        errno_assert(hndls[1] >= 0);
        char c = SIGNAL;
        int rc = chsend(sendch[1], &c, sizeof(c), -1);
        errno_assert(rc == 0);
        char signo;
        rc = chrecv(recvch[1], &signo, sizeof(signo), -1);
        errno_assert(rc == 0);
        assert(signo == SIGNAL);
        rc = hclose(hndls[0]);
        errno_assert(rc == 0);
        rc = hclose(hndls[1]);
        errno_assert(rc == 0);
    }

    rc = hclose(sendch[0]);
    errno_assert(rc == 0);
    rc = hclose(sendch[1]);
    errno_assert(rc == 0);
    rc = hclose(recvch[0]);
    errno_assert(rc == 0);
    rc = hclose(recvch[1]);
    errno_assert(rc == 0);

    return 0;
}

