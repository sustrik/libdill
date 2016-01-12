/*

  Copyright (c) 2015 Nir Soffer

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
#include <unistd.h>

#include "../libdill.h"

coroutine void relay(chan src, chan dst) {
    while(1) {
       int val;
       int rc = chr(src, &val, sizeof(val));
       assert(rc == 0);
       rc = chs(dst, &val, sizeof(val));
       assert(rc == 0);
    }
}

int main() {
    chan left = chmake(sizeof(int), 0);
    chan right = chmake(sizeof(int), 0);

    go(relay(left, right));
    go(relay(right, left));

    int val = 42;
    int rc = chs(left, &val, sizeof(val));
    assert(rc == 0);

    /* Fail with exit code 128+SIGALRM if we deadlock */
    alarm(1);

    msleep(1);

    return 0;
}

