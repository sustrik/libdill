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
    int b0 = btrace_attach(s[0], "s0");
    assert(b0 >= 0);
    int b1 = btrace_attach(s[1], "s1");
    assert(b1 >= 0);
    rc = bsend(b0, "\x03" "BC", 3, -1);
    assert(rc == 0);
    char buf[3];
    rc = brecv(b1, buf, 3, -1);
    assert(rc == 0);
    rc = hclose(b1);
    assert(rc == 0);
    rc = hclose(b0);
    assert(rc == 0);

    return 0;
}

