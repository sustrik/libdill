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
    s0 = nacl_attach(s0, key, 32, -1);
    errno_assert(s0 >= 0);
    s1 = nacl_attach(s1, key, 32, -1);
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
    rc = hclose(s1);
    errno_assert(rc == 0);
    rc = hclose(s0);
    errno_assert(rc == 0);

#if 0
    /* Test communication with wrong key. */
    rc = ipc_pair(s);
    assert(rc == 0);

    pfx0 = pfx_attach(log0, 2, 0);
    assert(pfx0 >= 0);
    pfx1 = pfx_attach(log1, 2, 0);
    assert(pfx1 >= 0);
    nacl0 = nacl_attach(pfx0, key, 32, -1);
    assert(nacl0 >= 0);
    nacl1 = nacl_attach(pfx1, badkey, 32, -1);
    assert(nacl1 >= 0);
    rc = msend(nacl0, "ABC", 3, -1);
    assert(rc == 0);
    sz = mrecv(nacl1, buf, sizeof(buf), -1);
    assert(sz == -1 && errno == EACCES);
    rc = hclose(nacl1);
    assert(rc == 0);
    rc = hclose(nacl0);
    assert(rc == 0);
#endif

    return 0;
}

