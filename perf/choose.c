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
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "../libdill.h"

static coroutine void worker(int ch) {
    int val;
    while(1) {
        chrecv(ch, &val, sizeof(val), -1);
        chsend(ch, &val, sizeof(val), -1);
    }
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("usage: choose <millions-of-roundtrips>\n");
        return 1;
    }
    long count = atol(argv[1]) * 1000000;

    int ch[2];
    chmake(ch);

    int64_t start = now();
    go(worker(ch[0]));

    int val = 0;
    long i;
    for(i = 0; i != count; ++i) {
        struct chclause clsout[] = {{CHSEND, ch[1], &val, sizeof(val)}};
        struct chclause clsin[] = {{CHRECV, ch[1], &val, sizeof(val)}};
        choose(clsout, 1, -1);
        choose(clsin, 1, -1);
    }

    int64_t stop = now();
    long duration = (long)(stop - start);
    long ns = (duration * 1000000) / (count * 2);

    printf("done %ldM roundtrips in %f seconds\n",
        (long)(count / 1000000), ((float)duration) / 1000);
    printf("duration of passing a single message: %ld ns\n", ns);
    printf("message passes per second: %fM\n",
        (float)(1000000000 / ns) / 1000000);

    return 0;
}

