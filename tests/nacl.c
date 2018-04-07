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

int main() {

    const char key[] = "01234567890123456789012345678901";
    const char badkey[] = "X1234567890123456789012345678901";
    
    /* Test normal communication. */
    int s[2];
    int rc = ipc_pair(s);
    errno_assert(rc == 0);
    int s0 = pfx_attach(s[0], 1, 0);
    errno_assert(s0 >= 0);
    int s1 = pfx_attach(s[1], 1, 0);
    errno_assert(s1 >= 0);
    struct nacl_storage mem;
    s0 = nacl_attach_mem(s0, key, &mem);
    errno_assert(s0 >= 0);
    s1 = nacl_attach(s1, key);
    errno_assert(s1 >= 0);
    rc = msend(s0, "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = msend(s0, "DEFG", 4, -1);
    errno_assert(rc == 0);
    char buf[10] = {0};
    ssize_t sz = mrecv(s1, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 3);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    sz = mrecv(s1, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 4);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F' && buf[3] == 'G');
    rc = msend(s1, "HIJ", 3, -1);
    errno_assert(rc == 0);
    sz = mrecv(s0, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 3);
    assert(buf[0] == 'H' && buf[1] == 'I' && buf[2] == 'J');
    s1 = nacl_detach(s1);
    errno_assert(s1 >= 0);
    s0 = nacl_detach(s0);
    errno_assert(s0 >= 0);
    rc = hclose(s1);
    errno_assert(rc == 0);
    rc = hclose(s0);
    errno_assert(rc == 0);

    /* Test wrong key. */
    rc = ipc_pair(s);
    errno_assert(rc == 0);
    s0 = pfx_attach(s[0], 1, 0);
    errno_assert(s0 >= 0);
    s1 = pfx_attach(s[1], 1, 0);
    errno_assert(s1 >= 0);
    s0 = nacl_attach(s0, key);
    errno_assert(s0 >= 0);
    s1 = nacl_attach(s1, badkey);
    errno_assert(s1 >= 0);
    rc = msend(s0, "ABC", 3, -1);
    errno_assert(rc == 0);
    sz = mrecv(s1, buf, sizeof(buf), -1);
    errno_assert(sz < 0 && errno == EACCES);
    rc = hclose(s1);
    errno_assert(rc == 0);
    rc = hclose(s0);
    errno_assert(rc == 0);

    /* Test sending via UDP. */
    struct ipaddr addr0;
    rc = ipaddr_local(&addr0, NULL, 5555, IPADDR_IPV4);
    errno_assert(rc == 0);
    s0 = udp_open(&addr0, NULL);
    errno_assert(s0 >= 0);
    struct ipaddr addr1;
    rc = ipaddr_remote(&addr1, "127.0.0.1", 5555, IPADDR_IPV4, -1);
    errno_assert(rc == 0);
    s1 = udp_open(NULL, &addr1);
    errno_assert(s1 >= 0);
    s0 = nacl_attach(s0, key);
    errno_assert(s0 >= 0);
    s1 = nacl_attach(s1, key);
    errno_assert(s1 >= 0);
    while(1) {
        rc = msend(s1, "ABC", 3, -1);
        errno_assert(rc == 0);
        char buf[16];
        ssize_t sz = mrecv(s0, buf, sizeof(buf), now() + 100);
        if(sz < 0 && errno == ETIMEDOUT)
            continue;
        errno_assert(sz >= 0);
        assert(sz == 3);
        assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
        break;
    }
    rc = hclose(s1);
    errno_assert(rc == 0);
    rc = hclose(s0);
    errno_assert(rc == 0);

    return 0;
}

