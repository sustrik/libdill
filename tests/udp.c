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

int main(void) {
    struct ipaddr addr1;
    int rc = ipaddr_local(&addr1, "127.0.0.1", 5555, 0);
    errno_assert(rc == 0);
    int s1 = udp_open(&addr1, NULL);
    errno_assert(s1 >= 0);
    struct ipaddr addr2;
    rc = ipaddr_local(&addr2, "127.0.0.1", 5556, 0);
    errno_assert(rc == 0);
    struct udp_storage mem;
    int s2 = udp_open_mem(&addr2, &addr1, &mem);
    errno_assert(s2 >= 0);

    struct ipaddr dst;
    rc = ipaddr_remote(&dst, "127.0.0.1", 5556, 0, -1);
    errno_assert(rc == 0);

    while(1) {
        rc = udp_send(s1, &dst, "ABC", 3);
        errno_assert(rc == 0);
        char buf[16];
        ssize_t sz = mrecv(s2, buf, sizeof(buf), now() + 100);
        if(sz < 0 && errno == ETIMEDOUT)
            continue;
        errno_assert(sz == 3);
        break;
    }

    while(1) {
        rc = msend(s2, "DEF", 3, -1);
        errno_assert(rc == 0);
        char buf[16];
        struct ipaddr addr;
        ssize_t sz = udp_recv(s1, &addr, buf, sizeof(buf), now() + 100);
        if(sz < 0 && errno == ETIMEDOUT)
            continue;
        errno_assert(sz == 3);
        break;
    }

    rc = hclose(s2);
    errno_assert(rc == 0);
    rc = hclose(s1);
    errno_assert(rc == 0);

    return 0;
}

