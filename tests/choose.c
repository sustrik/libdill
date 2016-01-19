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
#include <stdio.h>

#include "../libdill.h"

coroutine void sender1(chan ch, int val) {
    int rc = chsend(ch, &val, sizeof(val));
    assert(rc == 0);
    chclose(ch);
}

coroutine void sender2(chan ch, int val) {
    int rc = yield();
    assert(rc == 0);
    rc = chsend(ch, &val, sizeof(val));
    assert(rc == 0);
    chclose(ch);
}

coroutine void sender3(chan ch, int val, int64_t deadline) {
    int rc = msleep(deadline);
    assert(rc == 0);
    rc = chsend(ch, &val, sizeof(val));
    assert(rc == 0);
    chclose(ch);
}

coroutine void receiver1(chan ch, int expected) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val));
    assert(rc == 0);
    assert(val == expected);
    chclose(ch);
}

coroutine void receiver2(chan ch, int expected) {
    int rc = yield();
    assert(rc == 0);
    int val;
    rc = chrecv(ch, &val, sizeof(val));
    assert(rc == 0);
    assert(val == expected);
    chclose(ch);
}

coroutine void choosesender(chan ch, int val) {
    struct chclause cl = {ch, CHOOSE_CHSEND, &val, sizeof(val)};
    int rc = choose(&cl, 1, -1);
    assert(rc == 0);
    chclose(ch);
}

coroutine void feeder(chan ch, int val) {
    while(1) {
        int rc = chsend(ch, &val, sizeof(val));
        assert(rc == 0);
        rc = yield();
        assert(rc == 0);
    }
}

struct large {
    char buf[1024];
};

int main() {
    int rc;
    int val;

    /* In this test we are also going to make sure that the debugging support
       doesn't crash the application. */
    gotrace(1);

    /* Non-blocking receiver case. */
    chan ch1 = channel(sizeof(int), 0);
    go(sender1(chdup(ch1), 555));
    struct chclause cls1[] = {{ch1, CHOOSE_CHRECV, &val, sizeof(val)}};
    rc = choose(cls1, 1, -1);
    assert(rc == 0);
    assert(val == 555);
    chclose(ch1);

    /* Blocking receiver case. */
    chan ch2 = channel(sizeof(int), 0);
    go(sender2(chdup(ch2), 666));
    struct chclause cls2[] = {{ch2, CHOOSE_CHRECV, &val, sizeof(val)}};
    rc = choose(cls2, 1, -1);
    assert(rc == 0);
    assert(val == 666);
    chclose(ch2);

    /* Non-blocking sender case. */
    chan ch3 = channel(sizeof(int), 0);
    go(receiver1(chdup(ch3), 777));
    val = 777;
    struct chclause cls3[] = {{ch3, CHOOSE_CHSEND, &val, sizeof(val)}};
    rc = choose(cls3, 1, -1);
    assert(rc == 0);
    chclose(ch3);

    /* Blocking sender case. */
    chan ch4 = channel(sizeof(int), 0);
    go(receiver2(chdup(ch4), 888));
    val = 888;
    struct chclause cls4[] = {{ch4, CHOOSE_CHSEND, &val, sizeof(val)}};
    rc = choose(cls4, 1, -1);
    assert(rc == 0);
    chclose(ch4);

    /* Check with two channels. */
    chan ch5 = channel(sizeof(int), 0);
    chan ch6 = channel(sizeof(int), 0);
    go(sender1(chdup(ch6), 555));
    struct chclause cls5[] = {
        {ch5, CHOOSE_CHRECV, &val, sizeof(val)},
        {ch6, CHOOSE_CHRECV, &val, sizeof(val)}
    };
    rc = choose(cls5, 2, -1);
    assert(rc == 1);
    assert(val == 555);
    go(sender2(chdup(ch5), 666));
    rc = choose(cls5, 2, -1);
    assert(rc == 0);
    assert(val == 666);
    chclose(ch5);
    chclose(ch6);

    /* Test whether selection of in channels is random. */
    chan ch7 = channel(sizeof(int), 0);
    chan ch8 = channel(sizeof(int), 0);
    go(feeder(chdup(ch7), 111));
    go(feeder(chdup(ch8), 222));
    int i;
    int first = 0;
    int second = 0;
    int third = 0;
    for(i = 0; i != 100; ++i) {
        struct chclause cls6[] = {
            {ch7, CHOOSE_CHRECV, &val, sizeof(val)},
            {ch8, CHOOSE_CHRECV, &val, sizeof(val)}
        };
        rc = choose(cls6, 2, -1);
        assert(rc == 0 || rc == 1);
        if(rc == 0) {
            assert(val == 111);
            ++first;
        }
        if(rc == 1) {
            assert(val == 222);
            ++second;
        }
        int rc = yield();
        assert(rc == 0);
    }
    assert(first > 1 && second > 1);
    chclose(ch7);
    chclose(ch8);

    /* Test 'otherwise' clause. */
    chan ch9 = channel(sizeof(int), 0);
    struct chclause cls7[] = {{ch9, CHOOSE_CHRECV, &val, sizeof(val)}};
    rc = choose(cls7, 1, 0);
    assert(rc == -1 && errno == ETIMEDOUT);
    chclose(ch9);
    rc = choose(NULL, 0, 0);
    assert(rc == -1 && errno == ETIMEDOUT);

    /* Test two simultaneous senders vs. choose statement. */
    chan ch10 = channel(sizeof(int), 0);
    go(sender1(chdup(ch10), 888));
    go(sender1(chdup(ch10), 999));
    val = 0;
    struct chclause cls8[] = {{ch10, CHOOSE_CHRECV, &val, sizeof(val)}};
    rc = choose(cls8, 1, -1);
    assert(rc == 0);
    assert(val == 888);
    val = 0;
    rc = choose(cls8, 1, -1);
    assert(rc == 0);
    assert(val == 999);
    chclose(ch10);

    /* Test two simultaneous receivers vs. choose statement. */
    chan ch11 = channel(sizeof(int), 0);
    go(receiver1(chdup(ch11), 333));
    go(receiver1(chdup(ch11), 444));
    val = 333;
    struct chclause cls9[] = {{ch11, CHOOSE_CHSEND, &val, sizeof(val)}};
    rc = choose(cls9, 1, -1);
    assert(rc == 0);
    val = 444;
    rc = choose(cls9, 1, -1);
    assert(rc == 0);
    chclose(ch11);

    /* Choose vs. choose. */
    chan ch12 = channel(sizeof(int), 0);
    go(choosesender(chdup(ch12), 111));
    struct chclause cls10[] = {{ch12, CHOOSE_CHRECV, &val, sizeof(val)}};
    rc = choose(cls10, 1, -1);
    assert(rc == 0);
    assert(val == 111);
    chclose(ch12);

    /* Choose vs. buffered channels. */
    chan ch13 = channel(sizeof(int), 2);
    val = 999;
    struct chclause cls11[] = {{ch13, CHOOSE_CHSEND, &val, sizeof(val)}};
    rc = choose(cls11, 1, -1);
    assert(rc == 0);
    struct chclause cls12[] = {{ch13, CHOOSE_CHRECV, &val, sizeof(val)}};
    rc = choose(cls12, 1, -1);
    assert(rc == 0);
    assert(val == 999);
    chclose(ch13);

    /* Test whether allocating larger in buffer breaks previous in clause. */
    chan ch15 = channel(sizeof(struct large), 1);
    chan ch16 = channel(sizeof(int), 1);
    go(sender2(chdup(ch16), 1111));
    goredump();
    struct large lrg;
    struct chclause cls13[] = {
        {ch16, CHOOSE_CHRECV, &val, sizeof(val)},
        {ch15, CHOOSE_CHRECV, &lrg, sizeof(lrg)}
    };
    rc = choose(cls13, 2, -1);
    assert(rc == 0);
    assert(val == 1111);
    chclose(ch16);
    chclose(ch15);

    /* Test transferring a large object. */
    chan ch17 = channel(sizeof(struct large), 1);
    struct large large = {{0}};
    rc = chsend(ch17, &large, sizeof(large));
    assert(rc == 0);
    struct chclause cls14[] = {{ch17, CHOOSE_CHRECV, &lrg, sizeof(lrg)}};
    rc = choose(cls14, 1, -1);
    assert(rc == 0);
    chclose(ch17);

    /* Test that 'in' on done-with channel fires. */
    chan ch18 = channel(sizeof(int), 0);
    val = 2222;
    rc = chdone(ch18, &val, sizeof(val));
    assert(rc == 0);
    struct chclause cls15[] = {{ch18, CHOOSE_CHRECV, &val, sizeof(val)}};
    rc = choose(cls15, 1, -1);
    assert(rc == 0);
    assert(val == 2222);
    chclose(ch18);

    /* Test expiration of 'deadline' clause. */
    chan ch21 = channel(sizeof(int), 0);
    int64_t start = now();
    struct chclause cls17[] = {{ch21, CHOOSE_CHRECV, &val, sizeof(val)}};
    rc = choose(cls17, 1, start + 50);
    assert(rc == -1 && errno == ETIMEDOUT);
    int64_t diff = now() - start;
    assert(diff > 30 && diff < 70);
    chclose(ch21);

    /* Test unexpired 'deadline' clause. */
    chan ch22 = channel(sizeof(int), 0);
    start = now();
    go(sender3(chdup(ch22), 4444, start + 50));
    struct chclause cls18[] = {{ch22, CHOOSE_CHRECV, &val, sizeof(val)}};
    rc = choose(cls17, 1, start + 1000);
    assert(rc == 0);
    assert(val == 4444);
    diff = now() - start;
    assert(diff > 30 && diff < 70);
    chclose(ch22);

    return 0;
}

