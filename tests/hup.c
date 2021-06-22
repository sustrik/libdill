/*

  Copyright (c) 2021 Martin Sustrik, Tim Hewitt

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

#include <string.h>

#include "assert.h"
#include "../libdill.h"

coroutine void client(int s) {
    s = suffix_attach(s, "\r\n", 2);
    errno_assert(s >= 0);
    s = hup_attach(s, "STOP", 4);
    errno_assert(s >= 0);
    int rc = msend(s, "ABC", 3, -1);
    errno_assert(rc >= 0);
    /* HUP allows a half-closed connection that can still be listened on */
    rc = hup_done(s, -1);
    errno_assert(rc == 0);
    char buf[16];
    ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 3);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    s = hup_detach(s, -1);
    errno_assert(s >= 0);
    rc = hclose(s);
    errno_assert(rc >= 0);
}

coroutine void silent_client(int s)
{
    s = suffix_attach(s, "\r\n", 2);
    errno_assert(s >= 0);

    s = hup_attach(s, "STOP", 4);
    errno_assert(s >= 0);

    char buf[16];
    ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
    errno_assert(sz == 3);
    assert(buf[0] == 'G' && buf[1] == 'H' && buf[2] == 'I');

    s = hup_detach(s, -1);
    errno_assert(s >= 0);

    /*
     * expect the termination message from a noisy peer
     *
     * HUP peers _should_ read until EPIPE, to know that the peer has said
     * their piece. However, because HUP allows one side of a connection to
     * detach before the other side has stopped sending, situations like this
     * can arise. HUP is only useful as a building block in a larger protocol
     * that has enough context to coordinate protocol setup and teardown.
     */
    sz = mrecv(s, buf, sizeof(buf), -1);
    errno_assert(sz == 4);
    assert(buf[0] == 'S' && buf[1] == 'T' && buf[2] == 'O' && buf[3] == 'P');

    /* wait to allow the server to timeout before just closing the socket */
    int rc = msleep(now() + 200);

    rc = hclose(s);
    errno_assert(rc >= 0);
}

int main(void) {
    int p[2];
    int rc = ipc_pair(p);
    errno_assert(rc == 0);
    int cr = go(client(p[0]));
    errno_assert(cr >= 0);
    int s = suffix_attach(p[1], "\r\n", 2);
    errno_assert(s >= 0);
    s = hup_attach(s, "STOP", 4);
    errno_assert(s >= 0);
    char buf[16];
    ssize_t sz = mrecv(s, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 3);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = msend(s, "DEF", 3, -1);
    errno_assert(rc == 0);
    sz = mrecv(s, buf, sizeof(buf), -1);
    errno_assert(sz == -1 && errno == EPIPE);
    s = hup_detach(s, -1);
    errno_assert(s >= 0);
    rc = hclose(s);
    errno_assert(s >= 0);
    rc = bundle_wait(cr, -1);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    rc = ipc_pair(p);
    errno_assert(rc == 0);

    cr = go(silent_client(p[0]));
    errno_assert(cr >= 0);

    s = suffix_attach(p[1], "\r\n", 2);
    errno_assert(s >= 0);

    s = hup_attach(s, "STOP", 4);
    errno_assert(s >= 0);

    rc = msend(s, "GHI", 3, -1);
    errno_assert(rc == 0);

    rc = hup_done(s, -1);
    errno_assert(rc == 0);

    s = hup_detach(s, -1);
    errno_assert(s >= 0);

    /* don't expect termination message from quiet peer */
    sz = mrecv(s, buf, sizeof(buf), 100);
    assert(sz < 0);
    assert(errno == ETIMEDOUT);

    return 0;
}

