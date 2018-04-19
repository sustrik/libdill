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

#include "assert.h"
#include "../libdill.h"

coroutine void client(int s) {
    s = dtls_attach_client(s, -1);
    errno_assert(s >= 0);
    while(1) {
        struct iolist iol2 = {"C", 1, NULL, 0};
        struct iolist iol1 = {"AB", 2, &iol2, 0};
        int rc = msendl(s, &iol1, &iol2, -1);
        errno_assert(rc == 0);
        char buf[16];
        ssize_t sz = mrecv(s, buf, sizeof(buf), now() + 100);
        if(sz < 0 && errno == ETIMEDOUT) continue;
        errno_assert(sz >= 0);
        assert(sz == 3);
        assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
        break;
    }
    int rc = hclose(s);
    errno_assert(rc == 0);
}

void do_handshake(int s1, int s2) {
    int cr = go(client(s2));
    errno_assert(cr >= 0);
    s1 = dtls_attach_server(s1, "tests/cert.pem", "tests/key.pem", -1);
    errno_assert(s1 >= 0);
    char buf[16];
    ssize_t sz = mrecv(s1, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 3);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    while(1) {
        int rc = msend(s1, "DEF", 3, -1);
        errno_assert(rc == 0);
        rc = bundle_wait(cr, now() + 100);
        if(rc < 0 && errno == ETIMEDOUT) continue;
        errno_assert(rc == 0);
        break;
    }
    int rc = hclose(cr);
    errno_assert(rc == 0);
    rc = hclose(s1);
    errno_assert(rc == 0);
}

int main(void) {

    /* Test DTLS over UDP. */
    struct ipaddr src;
    int rc = ipaddr_remote(&src, "127.0.0.1", 5555, 0, -1);
    errno_assert(rc == 0);
    struct ipaddr dst;
    rc = ipaddr_remote(&dst, "127.0.0.1", 5556, 0, -1);
    errno_assert(rc == 0);
    int s1 = udp_open(&src, &dst);
    errno_assert(s1 >= 0);
    int s2 = udp_open(&dst, &src);
    errno_assert(s2 >= 0);
    do_handshake(s1, s2);

    /* Test DTLS over IPC/PREFIX. */
#if 0
    int sp[2];
    rc = ipc_pair(sp);
    errno_assert(rc == 0);
    sp[0] = prefix_attach(sp[0], 2, 0);
    errno_assert(sp[0] >= 0);
    sp[1] = prefix_attach(sp[1], 2, 0);
    errno_assert(sp[1] >= 0);
    do_handshake(sp[0], sp[1]);
#endif

    return 0;
}

