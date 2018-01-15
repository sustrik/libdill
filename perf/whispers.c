/*

  Copyright (c) 2015 Alex Cornejo

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

static coroutine void whisper(int left, int right) {
    int val;
    chrecv(right, &val, sizeof(val), -1);
    val++;
    chsend(left, &val, sizeof(val), -1);
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("usage: whispers <number-of-whispers>\n");
        return 1;
    }

    long count = atol(argv[1]);
    int64_t start = now();

    int left[2];
    int right[2];
    chmake(left);
    right[0] = left[0];
    right[1] = left[1];
    int leftmost = left[0];
    long i;
    for (i = 0; i < count; ++i) {
        chmake(right);
        go(whisper(left[1], right[0]));
        left[0] = right[0];
        left[1] = right[1];
    }

    int val = 1;
    chsend(right[1], &val, sizeof(val), -1);
    int res;
    chrecv(leftmost, &res, sizeof(res), -1);
    assert(res == count + 1);

    int64_t stop = now();
    long duration = (long)(stop - start);
    long ns = (duration * 1000000) / count;

    printf("took %f seconds\n", (float)duration / 1000);
    printf("performed %ld whispers in %f seconds\n", count, ((float)duration) / 1000);
    printf("duration of one whisper: %ld ns\n", ns);
    printf("whispers per second: %fM\n",
        (float)(1000000000 / ns) / 1000000);

    return 0;
}
