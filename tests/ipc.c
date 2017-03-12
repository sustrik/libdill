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

#include <sys/stat.h>
#include <unistd.h>

#include "assert.h"
#include "../libdill.h"

#define TESTADDR "ipc.test"

coroutine void client(void) {
    int cs = ipc_connect(TESTADDR, -1);
    errno_assert(cs >= 0);
    int rc = msleep(-1);
    errno_assert(rc == -1 && errno == ECANCELED);
    rc = hclose(cs);
    errno_assert(rc == 0);
}

coroutine void client2(int s) {
    char buf[3];
    int rc = brecv(s, buf, sizeof(buf), -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = bsend(s, "DEF", 3, -1);
    errno_assert(rc == 0);
    rc = ipc_close(s, -1);
    /* Main coroutine closes this coroutine before it manages to read
       the pending data from the socket. */
    errno_assert(rc == -1 && errno == ECANCELED);
}

coroutine void client3(int s) {
    char buf[3];
    int rc = brecv(s, buf, sizeof(buf), -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = brecv(s, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = bsend(s, "DEF", 3, -1);
    errno_assert(rc == 0);
    rc = ipc_close(s, -1);
    errno_assert(rc == 0);
}

coroutine void client4(int s) {
    /* This line should make us hit ipc pushback. */
    int rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = ipc_close(s, now() + 100);
    errno_assert(rc == -1 && errno == ETIMEDOUT);
}

int main() {
    char buf[16];

    /* Test deadline. */
    struct stat st;
    int rc = stat(TESTADDR, &st);
    if(rc == 0) errno_assert(unlink(TESTADDR) == 0);
    int ls = ipc_listen(TESTADDR, 10);
    errno_assert(ls >= 0);
    int cr = go(client());
    errno_assert(cr >= 0);
    int as = ipc_accept(ls, -1);
    errno_assert(as >= 0);
    int64_t deadline = now() + 30;
    ssize_t sz = sizeof(buf);
    rc = brecv(as, buf, sizeof(buf), deadline);
    errno_assert(rc == -1 && errno == ETIMEDOUT);
    int64_t diff = now() - deadline;
    assert(diff > -20 && diff < 20);
    rc = brecv(as, buf, sizeof(buf), deadline);
    errno_assert(rc == -1 && errno == ECONNRESET);
    rc = hclose(as);
    errno_assert(rc == 0);
    rc = hclose(ls);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Test simple data exchange. */
    int s[2];
    rc = ipc_pair(s);
    errno_assert(rc == 0);
    cr = go(client2(s[1]));
    errno_assert(cr >= 0);
    rc = bsend(s[0], "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = brecv(s[0], buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = brecv(s[0], buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = ipc_close(s[0], -1);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Manual termination handshake. */
    rc = ipc_pair(s);
    errno_assert(rc == 0);
    cr = go(client3(s[1]));
    errno_assert(cr >= 0);
    rc = bsend(s[0], "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = hdone(s[0]);
    errno_assert(rc == 0);
    rc = brecv(s[0], buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = brecv(s[0], buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = hclose(s[0]);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);    

    /* Emulate a DoS attack. */
    rc = ipc_pair(s);
    errno_assert(rc == 0);
    cr = go(client4(s[1]));
    errno_assert(cr >= 0);
    char buffer[2048];
    while(1) {
        rc = bsend(s[0], buffer, 2048, -1);
        if(rc == -1 && errno == ECONNRESET)
            break;
        errno_assert(rc == 0);
    }
    rc = ipc_close(s[0], -1);
    errno_assert(rc == -1 && errno == ECONNRESET);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Try some invalid inputs. */
    rc = ipc_pair(s);
    errno_assert(rc == 0);
    rc = bsendl(s[0], NULL, NULL, -1);
    errno_assert(rc == -1 && errno == EINVAL);
    struct iolist iol1 = {(void*)"ABC", 3, NULL, 0};
    struct iolist iol2 = {(void*)"DEF", 3, NULL, 0};
    rc = bsendl(s[0], &iol1, &iol2, -1);
    errno_assert(rc == -1 && errno == EINVAL);
    iol1.iol_next = &iol2;
    iol2.iol_next = &iol1;
    rc = bsendl(s[0], &iol1, &iol2, -1);
    errno_assert(rc == -1 && errno == EINVAL);
    assert(iol1.iol_rsvd == 0);
    assert(iol2.iol_rsvd == 0);
    rc = hclose(s[0]);
    errno_assert(rc == 0);
    rc = hclose(s[1]);
    errno_assert(rc == 0);

    return 0;
}

