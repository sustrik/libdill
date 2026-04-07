/*

  Copyright (c) 2017 Martin Sustrik

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

#include <libdill.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "assert.h"
#include "../libdill.h"

/* Coroutine that blocks in tcp_accept and reports the errno back
   through a channel when the accept completes (or fails). */
coroutine void blocked_accepter(int ls, int ch) {
    int as = tcp_accept(ls, NULL, -1);
    int err = errno;
    if(as >= 0) hclose(as);
    int rc = chsend(ch, &err, sizeof(err), -1);
    errno_assert(rc == 0);
}

/* Coroutine that connects and sends data, then waits to be cancelled. */
coroutine void connector(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    errno_assert(rc == 0);
    int cs = tcp_connect(&addr, now() + 1000);
    errno_assert(cs >= 0);
    rc = bsend(cs, "XYZ", 3, -1);
    errno_assert(rc == 0);
    rc = msleep(-1);
    errno_assert(rc == -1 && errno == ECANCELED);
    rc = hclose(cs);
    errno_assert(rc == 0);
}

int main(void) {
    int rc;

    /* Phase 1: Close a listener while a coroutine is blocked in tcp_accept.
       Before the fix, dill_pollset_clean returned EBUSY, leaving the fd's
       pollset cache entry with cached=1. The accept coroutine would hang
       or hit an assert. */
    struct ipaddr addr;
    rc = ipaddr_local(&addr, NULL, 0, 0);
    errno_assert(rc == 0);
    int ls = tcp_listen(&addr, 10);
    errno_assert(ls >= 0);

    int ch[2];
    rc = chmake(ch);
    errno_assert(rc == 0);
    int cr = go(blocked_accepter(ls, ch[0]));
    errno_assert(cr >= 0);

    /* Let the coroutine block in tcp_accept. */
    rc = msleep(now() + 50);
    errno_assert(rc == 0);

    /* Close the listener — triggers dill_pollset_clean which should
       cancel the blocked accept coroutine instead of returning EBUSY. */
    rc = hclose(ls);
    errno_assert(rc == 0);

    /* The accepter should have been woken with ECANCELED. */
    int err;
    rc = chrecv(ch[1], &err, sizeof(err), now() + 1000);
    errno_assert(rc == 0);
    assert(err == ECANCELED);

    rc = hclose(cr);
    errno_assert(rc == 0);
    rc = hclose(ch[0]);
    errno_assert(rc == 0);
    rc = hclose(ch[1]);
    errno_assert(rc == 0);

    /* Phase 2: Exercise fd-number reuse.
       The OS will likely hand back the same fd number that the old listener
       used. If the pollset cache was not properly cleared (the old bug),
       the new fd would be treated as already registered and either hang
       forever or hit an assert in kqueue/epoll. */
    rc = ipaddr_local(&addr, NULL, 0, 0);
    errno_assert(rc == 0);
    int ls2 = tcp_listen(&addr, 10);
    errno_assert(ls2 >= 0);
    int port = ipaddr_port(&addr);

    int cr2 = go(connector(port));
    errno_assert(cr2 >= 0);
    int as = tcp_accept(ls2, NULL, now() + 1000);
    errno_assert(as >= 0);

    /* Verify the reused fd is fully functional. */
    char buf[3];
    rc = brecv(as, buf, 3, now() + 1000);
    errno_assert(rc == 0);
    assert(buf[0] == 'X' && buf[1] == 'Y' && buf[2] == 'Z');

    rc = hclose(as);
    errno_assert(rc == 0);
    rc = hclose(cr2);
    errno_assert(rc == 0);
    rc = hclose(ls2);
    errno_assert(rc == 0);

    return 0;
}