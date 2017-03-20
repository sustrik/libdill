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

#include <assert.h>

#include "../libdill.h"

int main() {

    int s[2];
    int rc = ipc_pair(s);
    assert(rc == 0);
    int pfx0 = pfx_attach(s[0]);
    assert(pfx0 >= 0);
    int pfx1 = pfx_attach(s[1]);
    assert(pfx1 >= 0);
    int m0 = mtrace_attach(pfx0, "pfx0");
    assert(m0 >= 0);
    int m1 = mtrace_attach(pfx1, "pfx1");
    assert(m1 >= 0);
    rc = msend(m0, "\x03" "BC", 3, -1);
    assert(rc == 0);
    char buf[3];
    ssize_t sz = mrecv(m1, buf, 3, -1);
    assert(sz == 3);
    rc = hclose(m1);
    assert(rc == 0);
    rc = hclose(m0);
    assert(rc == 0);

    return 0;
}

