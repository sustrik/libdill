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

#include <unistd.h>

#include "assert.h"
#include "../libdill.h"

coroutine void relay(int src, int dst) {
    while(1) {
       int val;
       int rc = chrecv(src, &val, sizeof(val), -1);
       if(rc == -1 && errno == ECANCELED) return;
       errno_assert(rc == 0);
       rc = chsend(dst, &val, sizeof(val), -1);
       if(rc == -1 && errno == ECANCELED) return;
       errno_assert(rc == 0);
    }
}

int main() {
    int left = chmake(sizeof(int));
    errno_assert(left >= 0);
    int right = chmake(sizeof(int));
    errno_assert(right >= 0);
    int hndls[2];
    hndls[0] = go(relay(left, right));
    errno_assert(hndls[0] >= 0);
    hndls[1] = go(relay(right, left));
    errno_assert(hndls[1] >= 0);
    int val = 42;
    int rc = chsend(left, &val, sizeof(val), -1);
    errno_assert(rc == 0);
    /* Fail with exit code 128+SIGALRM if we deadlock */
    alarm(1);
    hclose(hndls[0]);
    hclose(hndls[1]);
    hclose(left);
    hclose(right);

    return 0;
}

