/*

  Copyright (c) 2015 Martin Sustrik

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
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#include "../libdill.h"

coroutine static void delay(int n, chan ch) {
    int rc = msleep(now() + n);
    assert(rc == 0);
    rc = chsend(ch, &n, sizeof(n), -1);
    assert(rc == 0);
}

int main() {
    /* Test 'msleep'. */
    int64_t deadline = now() + 100;
    int rc = msleep(deadline);
    assert(rc == 0);
    int64_t diff = now () - deadline;
    assert(diff > -20 && diff < 20);

    /* msleep-sort */
    chan ch = channel(sizeof(int), 0);
    assert(ch);
    handle hndls[4];
    hndls[0] = go(delay(30, ch));
    assert(hndls[0]);
    hndls[1] = go(delay(40, ch));
    assert(hndls[1]);
    hndls[2] = go(delay(10, ch));
    assert(hndls[2]);
    hndls[3] = go(delay(20, ch));
    assert(hndls[3]);
    int val;
    rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 10);
    rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 20);
    rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 30);
    rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == 40);
    gocancel(hndls, 4, 1000);

    return 0;
}

