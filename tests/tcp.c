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

#include "assert.h"
#include "../libdill.h"

coroutine void client(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    errno_assert(rc == 0);
    int cs = tcp_connect(&addr, -1);
    errno_assert(cs >= 0);
    rc = msleep(-1);
    errno_assert(rc == -1 && errno == ECANCELED);
    rc = hclose(cs);
    errno_assert(rc == 0);
}

coroutine void client2(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    errno_assert(rc == 0);
    int cs = tcp_connect(&addr, -1);
    errno_assert(cs >= 0);
    char buf[3];
    rc = brecv(cs, buf, sizeof(buf), -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = bsend(cs, "DEF", 3, -1);
    errno_assert(rc == 0);
    rc = tcp_close(cs, -1);
    /* Main coroutine closes this coroutine before it manages to read
       the pending data from the socket. */
    errno_assert(rc == -1 && errno == ECANCELED);
}

coroutine void client3(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    errno_assert(rc == 0);
    int cs = tcp_connect(&addr, -1);
    errno_assert(cs >= 0);
    char buf[3];
    rc = brecv(cs, buf, sizeof(buf), -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = brecv(cs, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = bsend(cs, "DEF", 3, -1);
    errno_assert(rc == 0);
    rc = tcp_close(cs, -1);
    errno_assert(rc == 0);
}

coroutine void client4(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    errno_assert(rc == 0);
    int cs = tcp_connect(&addr, -1);
    errno_assert(cs >= 0);
    /* This line should make us hit TCP pushback. */
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = tcp_close(cs, now() + 100);
    errno_assert(rc == -1 && errno == ETIMEDOUT);
}

int main(void) {
    char buf[16];

    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, 5555, 0);
    errno_assert(rc == 0);

    /* Test deadline. */
    int ls = tcp_listen(&addr, 10);
    errno_assert(ls >= 0);
    int cr = go(client(5555));
    errno_assert(cr >= 0);
    int as = tcp_accept(ls, NULL, -1);
    errno_assert(as >= 0);
    rc = mrecv(as, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == ENOTSUP);
    rc = msend(as, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == ENOTSUP);
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
    ls = tcp_listen(&addr, 10);
    errno_assert(ls >= 0);
    cr = go(client2(5555));
    errno_assert(cr >= 0);
    as = tcp_accept(ls, NULL, -1);
    errno_assert(as >= 0);
    rc = bsend(as, "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = brecv(as, buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = brecv(as, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = tcp_close(as, -1);
    errno_assert(rc == 0);
    rc = hclose(ls);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Manual termination handshake. */
    ls = tcp_listen(&addr, 10);
    errno_assert(ls >= 0);
    cr = go(client3(5555));
    errno_assert(cr >= 0);
    as = tcp_accept(ls, NULL, -1);
    errno_assert(as >= 0);
    rc = bsend(as, "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = hdone(as);
    errno_assert(rc == 0);
    rc = brecv(as, buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = brecv(as, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = hclose(as);
    errno_assert(rc == 0);
    rc = hclose(ls);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);    

    /* Emulate a DoS attack. */
    ls = tcp_listen(&addr, 10);
    cr = go(client4(5555));
    errno_assert(cr >= 0);
    as = tcp_accept(ls, NULL, -1);
    errno_assert(as >= 0);
    char buffer[2048];
    while(1) {
        rc = bsend(as, buffer, 2048, -1);
        if(rc == -1 && errno == ECONNRESET)
            break;
        errno_assert(rc == 0);
    }
    rc = tcp_close(as, -1);
    errno_assert(rc == -1 && errno == ECONNRESET);
    rc = hclose(ls);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Test ephemeral ports. */
    rc = ipaddr_local(&addr, NULL, 0, 0);
    errno_assert(rc == 0);
    ls = tcp_listen(&addr, 10);
    errno_assert(ls >= 0);
    int port = ipaddr_port(&addr);
    assert(port > 0);
    rc = hclose(ls);
    errno_assert(rc == 0);

    return 0;
}
  
