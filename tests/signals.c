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

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

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

coroutine int sender(chan ch) {
    char signo;
    int rc = chrecv(ch, &signo, sizeof(signo), -1);
    assert(rc == 0);
    rc = kill(getpid(), signo);
    assert(rc == 0);
    return 0;
}

coroutine int receiver(chan ch) {
    int events = fdwait(signal_pipe[0], FDW_IN, -1);
    assert(events == FDW_IN);

    char signo;
    ssize_t sz = read(signal_pipe[0], &signo, 1);
    assert(sz == 1);
    assert(signo == SIGNAL);

    int rc = chsend(ch, &signo, sizeof(signo), -1);
    assert(rc == 0);

    return 0;
}

int main() {
    int err = pipe(signal_pipe);
    assert(err == 0);

    signal(SIGNAL, signal_intr);

    chan sendch = channel(sizeof(char), 0);
    assert(sendch);
    chan recvch = channel(sizeof(char), 0);
    assert(recvch);

    int i;
    for(i = 0; i < COUNT; ++i) {
        int hndls[2];
        hndls[0] = go(sender(sendch));
        assert(hndls[0] >= 0);
        hndls[1] = go(receiver(recvch));
        assert(hndls[1] >= 0);
        char c = SIGNAL;
        int rc = chsend(sendch, &c, sizeof(c), -1);
        assert(rc == 0);
        char signo;
        rc = chrecv(recvch, &signo, sizeof(signo), -1);
        assert(rc == 0);
        assert(signo == SIGNAL);
        rc = hwait(hndls[0], NULL, -1);
        assert(rc == 0);
        rc = hwait(hndls[1], NULL, -1);
        assert(rc == 0);
    }

    return 0;
}

