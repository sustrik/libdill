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

#include <string.h>

#include "assert.h"
#include "../libdill.h"

int client_connect(void) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", 5555, 0, -1);
    errno_assert(rc == 0);
    int s = tls_connect(&addr, -1);
    errno_assert(s >= 0);
    return s;
}

coroutine void client1(void) {
    int s = client_connect();
    int rc = bsend(s, "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = hdone(s, -1);
    errno_assert(rc == 0);
    char buf[3];
    rc = brecv(s, buf, sizeof(buf), -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = brecv(s, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = hclose(s);
    errno_assert(rc == 0);
}

coroutine void client2(void) {
    int s = client_connect();
    int rc = bsend(s, "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = tls_close(s, -1);
    errno_assert(rc == 0);
}

int main(void) {
    char buf[16];

    /* Prologue. */
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, 5555, 0);
    errno_assert(rc == 0);
    int ls = tls_listen(&addr, "tests/cert.pem", "tests/key.pem", 10);
    errno_assert(ls >= 0);

    /* Test simple data exchange, terminated by explicit handshake. */
    int cr = go(client1());
    errno_assert(cr >= 0);
    int s = tls_accept(ls, NULL, -1);
    errno_assert(s >= 0);
    rc = brecv(s, buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = brecv(s, buf, 3, -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = bsend(s, "DEF", 3, -1);
    errno_assert(rc == 0);
    rc = hdone(s, -1);
    errno_assert(rc == 0);
    rc = hclose(s);
    errno_assert(rc == 0);
    rc = hdone(cr, -1);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Test simple data transfer, terminated by tls_close(). */
    cr = go(client2());
    errno_assert(cr >= 0);
    s = tls_accept(ls, NULL, -1);
    errno_assert(s >= 0);
    rc = brecv(s, buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = tls_close(s, -1);
    errno_assert(rc == 0);
    rc = hdone(cr, -1);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Epilogue. */
    rc = hclose(ls);
    errno_assert(rc == 0);

    return 0;
}

