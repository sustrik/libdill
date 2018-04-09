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

coroutine void client(void) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", 5555, 0, -1);
    errno_assert(rc == 0);
    int s = tcp_connect(&addr, -1);
    errno_assert(s >= 0);

    int cs = prefix_attach(s, 8, 0);
    errno_assert(cs >= 0);
    rc = msend(cs, "ABC", 3, -1);
    errno_assert(rc == 0);
    char buf[3];
    ssize_t sz = mrecv(cs, buf, 3, -1);
    errno_assert(sz == 3);
    assert(buf[0] == 'G' && buf[1] == 'H' && buf[2] == 'I');
    rc = msend(cs, "DEF", 3, -1);
    errno_assert(rc == 0);
    int ts = prefix_detach(cs);
    errno_assert(ts >= 0);
    rc = hclose(ts);
    errno_assert(rc == 0);
}

int main(void) {
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, 5555, 0);
    errno_assert(rc == 0);
    int ls = tcp_listen(&addr, 10);
    errno_assert(ls >= 0);
    int clh = go(client());
    int as = tcp_accept(ls, NULL, -1);
    errno_assert(as >= 0);

    int cs = prefix_attach(as, 8, 0);
    errno_assert(cs >= 0);
    char buf[16];
    ssize_t sz = mrecv(cs, buf, sizeof(buf), -1);
    errno_assert(sz == 3);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = msend(cs, "GHI", 3, -1);
    errno_assert(rc == 0);
    sz = mrecv(cs, buf, sizeof(buf), -1);
    errno_assert(sz == 3);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    int ts = prefix_detach(cs);
    errno_assert(ts >= 0);
    rc = hclose(ts);
    errno_assert(rc == 0);
    rc = bundle_wait(clh, -1);
    errno_assert(rc == 0);
    rc = hclose(clh);
    errno_assert(rc == 0);

    int h[2];
    rc = ipc_pair(h);
    errno_assert(rc == 0);
    int s0 = prefix_attach(h[0], 3, PREFIX_LITTLE_ENDIAN);
    errno_assert(s0 >= 0);
    struct prefix_storage mem;
    int s1 = prefix_attach_mem(h[1], 3, PREFIX_LITTLE_ENDIAN, &mem);
    errno_assert(s1 >= 0);
    rc = msend(s0, "First", 5, -1);
    errno_assert(rc == 0);
    rc = msend(s0, "Second", 6, -1);
    errno_assert(rc == 0);
    rc = msend(s0, "Third", 5, -1);
    errno_assert(rc == 0);
    sz = mrecv(s1, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 5 && memcmp(buf, "First", 5) == 0);
    sz = mrecvl(s1, NULL, NULL, -1);
    errno_assert(sz >= 0);
    sz = mrecv(s1, buf, sizeof(buf), -1);
    errno_assert(sz >= 0);
    assert(sz == 5 && memcmp(buf, "Third", 5) == 0);
    rc = msend(s1, "Red", 3, -1);
    errno_assert(rc == 0);
    rc = msend(s1, "Blue", 4, -1);
    errno_assert(rc == 0);
    int ts1 = prefix_detach(s1);
    errno_assert(ts1 >= 0);
    sz = mrecv(s0, buf, sizeof(buf), -1);
    errno_assert(sz == 3 && memcmp(buf, "Red", 3) == 0);
    sz = mrecv(s0, buf, sizeof(buf), -1);
    errno_assert(sz == 4 && memcmp(buf, "Blue", 4) == 0);
    int ts0 = prefix_detach(s0);
    errno_assert(ts0 >= 0);
    rc = hclose(ts1);
    errno_assert(rc == 0);
    rc = hclose(ts0);
    errno_assert(rc == 0);
    rc = hclose(ls);
    errno_assert(rc == 0);

    return 0;
}

