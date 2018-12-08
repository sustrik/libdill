/*

  Copyright (c) 2018 Martin Sustrik

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

coroutine void client1(int s) {
    int rc = tls_attach_client(s, NULL, -1);
    errno_assert(rc == 0);
    rc = bsend(s, "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = tls_done(s, -1);
    errno_assert(rc == 0);
    char buf[3];
    rc = brecv(s, buf, sizeof(buf), -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = tls_detach(s, -1);
    errno_assert(rc == 0);
    rc = hclose(s);
    errno_assert(rc == 0);
}

coroutine void client2(int s) {
    int rc = tls_attach_client(s, NULL, -1);
    errno_assert(rc == 0);
    rc = bsend(s, "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = tls_detach(s, -1);
    errno_assert(rc == 0);
    rc = bsend(s, "DEF", 3, -1);
    errno_assert(rc == 0);
    rc = ipc_close(s, -1);
    errno_assert(rc == 0);
}

coroutine void client3(int s) {
    int rc = tls_attach_client(s, NULL, -1);
    errno_assert(rc == 0);
    uint8_t c = 0;
    uint8_t b[2777];
    int i;
    for(i = 0; i != 257; ++i) {
        int j;
        for(j = 0; j != sizeof(b); j++) {
            b[j] = c;
            c++;
        }
        int rc = bsend(s, b, sizeof(b), -1);
        errno_assert(rc == 0);
    }
    rc = tls_detach(s, -1);
    errno_assert(rc == 0);
    rc = hclose(s);
    errno_assert(rc == 0);
}

int main(void) {
    char buf[16];

    /* Test simple data exchange, with explicit handshake. */
    int p[2];
    int rc = ipc_pair(p, NULL, NULL);
    errno_assert(rc == 0);
    int cr = go(client1(p[1]));
    errno_assert(cr >= 0);
    rc = tls_attach_server(p[0], "tests/cert.pem", "tests/key.pem", NULL, -1);
    errno_assert(rc == 0);
    rc = brecv(p[0], buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = brecv(p[0], buf, 3, -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = bsend(p[0], "DEF", 3, -1);
    errno_assert(rc == 0);
    rc = tls_done(p[0], -1);
    errno_assert(rc == 0);
    rc = tls_detach(p[0], -1);
    errno_assert(rc == 0);
    rc = hclose(p[0]);
    errno_assert(rc == 0);
    rc = bundle_wait(cr, -1);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Test simple data transfer, terminated by tls_detach().
       Then send some data over undelying IPC connection. */
    rc = ipc_pair(p, NULL, NULL);
    errno_assert(rc == 0);
    cr = go(client2(p[1]));
    errno_assert(cr >= 0);
    rc = tls_attach_server(p[0], "tests/cert.pem", "tests/key.pem", NULL, -1);
    errno_assert(rc == 0);
    rc = brecv(p[0], buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = tls_detach(p[0], -1);
    errno_assert(rc == 0);
    rc = brecv(p[0], buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = ipc_close(p[0], -1);
    errno_assert(rc == 0);
    rc = bundle_wait(cr, -1);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Transfer large amount of data. */
    rc = ipc_pair(p, NULL, NULL);
    errno_assert(rc == 0);
    cr = go(client3(p[1]));
    errno_assert(cr >= 0);
    rc = tls_attach_server(p[0], "tests/cert.pem", "tests/key.pem", NULL, -1);
    errno_assert(rc == 0);
    uint8_t c = 0;
    int i;
    for(i = 0; i != 2777; ++i) {
        uint8_t b[257];
        rc = brecv(p[0], b, sizeof(b), -1);
        errno_assert(rc == 0);
        int j;
        for(j = 0; j != sizeof(b); ++j) {
            assert(b[j] == c);
            c++;
        }
    }
    rc = tls_detach(p[0], -1);
    errno_assert(rc == 0);
    rc = bundle_wait(cr, -1);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);
    rc = hclose(p[0]);
    errno_assert(rc == 0);
    
    return 0;
}

