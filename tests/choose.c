/*

  Copyright (c) 2016 Martin Sustrik

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

coroutine int sender1(chan ch, int val) {
    int rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    return 0;
}

coroutine int sender2(chan ch, int val) {
    int rc = yield();
    assert(rc == 0);
    rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    return 0;
}

coroutine int sender3(chan ch, int val, int64_t deadline) {
    int rc = msleep(deadline);
    assert(rc == 0);
    rc = chsend(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    return 0;
}

coroutine int receiver1(chan ch, int expected) {
    int val;
    int rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == expected);
    return 0;
}

coroutine int receiver2(chan ch, int expected) {
    int rc = yield();
    assert(rc == 0);
    int val;
    rc = chrecv(ch, &val, sizeof(val), -1);
    assert(rc == 0);
    assert(val == expected);
    return 0;
}

coroutine int choosesender(chan ch, int val) {
    struct chclause cl = {ch, CHSEND, &val, sizeof(val)};
    int rc = choose(&cl, 1, -1);
    assert(rc == 0);
    return 0;
}

coroutine int feeder(chan ch, int val) {
    while(1) {
        int rc = chsend(ch, &val, sizeof(val), -1);
        if(rc == -1 && errno == ECANCELED) return 0;
        assert(rc == 0);
        rc = yield();
        if(rc == -1 && errno == ECANCELED) return 0;
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
       doesn't hndlash the application. */
    gotrace(1);

    /* Non-blocking receiver case. */
    chan ch1 = channel(sizeof(int), 0);
    assert(ch1);
    int hndl1 = go(sender1(ch1, 555));
    assert(hndl1 >= 0);
    struct chclause cls1[] = {{ch1, CHRECV, &val, sizeof(val)}};
    rc = choose(cls1, 1, -1);
    assert(rc == 0);
    assert(val == 555);
    chclose(ch1);
    rc = hwait(hndl1, NULL, -1);
    assert(rc == 0);

    /* Blocking receiver case. */
    chan ch2 = channel(sizeof(int), 0);
    assert(ch2);
    int hndl2 = go(sender2(ch2, 666));
    assert(hndl2 >= 0);
    struct chclause cls2[] = {{ch2, CHRECV, &val, sizeof(val)}};
    rc = choose(cls2, 1, -1);
    assert(rc == 0);
    assert(val == 666);
    chclose(ch2);
    rc = hwait(hndl2, NULL, -1);
    assert(rc == 0);

    /* Non-blocking sender case. */
    chan ch3 = channel(sizeof(int), 0);
    assert(ch3);
    int hndl3 = go(receiver1(ch3, 777));
    assert(hndl3 >= 0);
    val = 777;
    struct chclause cls3[] = {{ch3, CHSEND, &val, sizeof(val)}};
    rc = choose(cls3, 1, -1);
    assert(rc == 0);
    chclose(ch3);
    rc = hwait(hndl3, NULL, -1);
    assert(rc == 0);

    /* Blocking sender case. */
    chan ch4 = channel(sizeof(int), 0);
    assert(ch4);
    int hndl4 = go(receiver2(ch4, 888));
    assert(hndl4 >= 0);
    val = 888;
    struct chclause cls4[] = {{ch4, CHSEND, &val, sizeof(val)}};
    rc = choose(cls4, 1, -1);
    assert(rc == 0);
    chclose(ch4);
    rc = hwait(hndl4, NULL, -1);
    assert(rc == 0);

    /* Check with two channels. */
    int hndl5[2];
    chan ch5 = channel(sizeof(int), 0);
    assert(ch5);
    chan ch6 = channel(sizeof(int), 0);
    assert(ch6);
    hndl5[0] = go(sender1(ch6, 555));
    assert(hndl5 >= 0);
    struct chclause cls5[] = {
        {ch5, CHRECV, &val, sizeof(val)},
        {ch6, CHRECV, &val, sizeof(val)}
    };
    rc = choose(cls5, 2, -1);
    assert(rc == 1);
    assert(val == 555);
    hndl5[1] = go(sender2(ch5, 666));
    assert(hndl5 >= 0);
    rc = choose(cls5, 2, -1);
    assert(rc == 0);
    assert(val == 666);
    chclose(ch5);
    chclose(ch6);
    rc = hwait(hndl5[0], NULL, -1);
    assert(rc == 0);
    rc = hwait(hndl5[1], NULL, -1);
    assert(rc == 0);

    /* Test whether selection of in channels is random. */
    chan ch7 = channel(sizeof(int), 0);
    assert(ch7);
    chan ch8 = channel(sizeof(int), 0);
    assert(ch8);
    int hndl6[2];
    hndl6[0] = go(feeder(ch7, 111));
    assert(hndl6[0] >= 0);
    hndl6[1] = go(feeder(ch8, 222));
    assert(hndl6[1] >= 0);
    int i;
    int first = 0;
    int second = 0;
    int third = 0;
    for(i = 0; i != 100; ++i) {
        struct chclause cls6[] = {
            {ch7, CHRECV, &val, sizeof(val)},
            {ch8, CHRECV, &val, sizeof(val)}
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
    hclose(hndl6[0]);
    hclose(hndl6[1]);
    chclose(ch7);
    chclose(ch8);

    /* Test 'otherwise' clause. */
    chan ch9 = channel(sizeof(int), 0);
    assert(ch9);
    struct chclause cls7[] = {{ch9, CHRECV, &val, sizeof(val)}};
    rc = choose(cls7, 1, 0);
    assert(rc == -1 && errno == ETIMEDOUT);
    chclose(ch9);
    rc = choose(NULL, 0, 0);
    assert(rc == -1 && errno == ETIMEDOUT);

    /* Test two simultaneous senders vs. choose statement. */
    chan ch10 = channel(sizeof(int), 0);
    assert(ch10);
    int hndl7[2];
    hndl7[0] = go(sender1(ch10, 888));
    assert(hndl7[0] >= 0);
    hndl7[1] = go(sender1(ch10, 999));
    assert(hndl7[1] >= 0);
    val = 0;
    struct chclause cls8[] = {{ch10, CHRECV, &val, sizeof(val)}};
    rc = choose(cls8, 1, -1);
    assert(rc == 0);
    assert(val == 888);
    val = 0;
    rc = choose(cls8, 1, -1);
    assert(rc == 0);
    assert(val == 999);
    chclose(ch10);
    rc = hwait(hndl7[0], NULL, -1);
    assert(rc == 0);
    rc = hwait(hndl7[1], NULL, -1);
    assert(rc == 0);

    /* Test two simultaneous receivers vs. choose statement. */
    chan ch11 = channel(sizeof(int), 0);
    assert(ch11);
    int hndl8[2];
    hndl8[0] = go(receiver1(ch11, 333));
    assert(hndl8[0] >= 0);
    hndl8[1] = go(receiver1(ch11, 444));
    assert(hndl8[1] >= 0);
    val = 333;
    struct chclause cls9[] = {{ch11, CHSEND, &val, sizeof(val)}};
    rc = choose(cls9, 1, -1);
    assert(rc == 0);
    val = 444;
    rc = choose(cls9, 1, -1);
    assert(rc == 0);
    chclose(ch11);
    rc = hwait(hndl8[0], NULL, -1);
    assert(rc == 0);
    rc = hwait(hndl8[1], NULL, -1);
    assert(rc == 0);

    /* Choose vs. choose. */
    chan ch12 = channel(sizeof(int), 0);
    assert(ch12);
    int hndl9 = go(choosesender(ch12, 111));
    assert(hndl9 >= 0);
    struct chclause cls10[] = {{ch12, CHRECV, &val, sizeof(val)}};
    rc = choose(cls10, 1, -1);
    assert(rc == 0);
    assert(val == 111);
    chclose(ch12);
    rc = hwait(hndl9, NULL, -1);
    assert(rc == 0);

    /* Choose vs. buffered channels. */
    chan ch13 = channel(sizeof(int), 2);
    assert(ch13);
    val = 999;
    struct chclause cls11[] = {{ch13, CHSEND, &val, sizeof(val)}};
    rc = choose(cls11, 1, -1);
    assert(rc == 0);
    struct chclause cls12[] = {{ch13, CHRECV, &val, sizeof(val)}};
    rc = choose(cls12, 1, -1);
    assert(rc == 0);
    assert(val == 999);
    chclose(ch13);

    /* Test whether allocating larger in buffer breaks previous in clause. */
    chan ch15 = channel(sizeof(struct large), 1);
    assert(ch15);
    chan ch16 = channel(sizeof(int), 1);
    assert(ch16);
    int hndl10 = go(sender2(ch16, 1111));
    assert(hndl10 >= 0);
    goredump();
    struct large lrg;
    struct chclause cls13[] = {
        {ch16, CHRECV, &val, sizeof(val)},
        {ch15, CHRECV, &lrg, sizeof(lrg)}
    };
    rc = choose(cls13, 2, -1);
    assert(rc == 0);
    assert(val == 1111);
    chclose(ch16);
    chclose(ch15);
    rc = hwait(hndl10, NULL, -1);
    assert(rc == 0);

    /* Test transferring a large object. */
    chan ch17 = channel(sizeof(struct large), 1);
    assert(ch17);
    struct large large = {{0}};
    rc = chsend(ch17, &large, sizeof(large), -1);
    assert(rc == 0);
    struct chclause cls14[] = {{ch17, CHRECV, &lrg, sizeof(lrg)}};
    rc = choose(cls14, 1, -1);
    assert(rc == 0);
    chclose(ch17);

    /* Test that 'in' on done-with channel fires. */
    chan ch18 = channel(sizeof(int), 0);
    assert(ch18);
    rc = chdone(ch18);
    assert(rc == 0);
    struct chclause cls15[] = {{ch18, CHRECV, &val, sizeof(val)}};
    rc = choose(cls15, 1, -1);
    assert(rc == 0 && errno == EPIPE);
    chclose(ch18);

    /* Test expiration of 'deadline' clause. */
    chan ch21 = channel(sizeof(int), 0);
    assert(ch21);
    int64_t start = now();
    struct chclause cls17[] = {{ch21, CHRECV, &val, sizeof(val)}};
    rc = choose(cls17, 1, start + 50);
    assert(rc == -1 && errno == ETIMEDOUT);
    int64_t diff = now() - start;
    assert(diff > 30 && diff < 70);
    chclose(ch21);

    /* Test unexpired 'deadline' clause. */
    chan ch22 = channel(sizeof(int), 0);
    assert(ch22);
    start = now();
    int hndl11 = go(sender3(ch22, 4444, start + 50));
    assert(hndl11 >= 0);
    struct chclause cls18[] = {{ch22, CHRECV, &val, sizeof(val)}};
    rc = choose(cls17, 1, start + 1000);
    assert(rc == 0);
    assert(val == 4444);
    diff = now() - start;
    assert(diff > 30 && diff < 70);
    chclose(ch22);
    rc = hwait(hndl11, NULL, -1);
    assert(rc == 0);

    return 0;
}

