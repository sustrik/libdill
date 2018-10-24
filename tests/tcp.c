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
#include <libdill.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>

#include <libdill.h>

#include "assert.h"
#include "../libdill.h"

coroutine void client(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    errno_assert(rc == 0);
    int cs = tcp_connect(&addr, -1);
    errno_assert(cs >= 0);
    rc = msleep(-1);
    errno_assert(rc == -1 && errno == ECANCELED);
    rc = hclose(cs);
    errno_assert(rc == 0);
}

coroutine void client2(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    errno_assert(rc == 0);
    int cs = tcp_connect(&addr, -1);
    errno_assert(cs >= 0);
    char buf[3];
    rc = brecv(cs, buf, sizeof(buf), -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = bsend(cs, "DEF", 3, -1);
    errno_assert(rc == 0);
    rc = tcp_close(cs, -1);
    /* Main coroutine closes this coroutine before it manages to read
       the pending data from the socket. */
    errno_assert(rc == -1 && errno == ECANCELED);
}

coroutine void client3(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    errno_assert(rc == 0);
    int cs = tcp_connect(&addr, -1);
    errno_assert(cs >= 0);
    char buf[3];
    rc = brecv(cs, buf, sizeof(buf), -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'A' && buf[1] == 'B' && buf[2] == 'C');
    rc = brecv(cs, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = bsend(cs, "DEF", 3, -1);
    errno_assert(rc == 0);
    rc = tcp_close(cs, -1);
    errno_assert(rc == 0);
}

coroutine void client4(int port) {
    struct ipaddr addr;
    int rc = ipaddr_remote(&addr, "127.0.0.1", port, 0, -1);
    errno_assert(rc == 0);
    int cs = tcp_connect(&addr, -1);
    errno_assert(cs >= 0);
    /* This line should make us hit TCP pushback. */
    rc = msleep(now() + 100);
    errno_assert(rc == 0);
    rc = tcp_close(cs, now() + 100);
    errno_assert(rc == -1 && errno == ETIMEDOUT);
}

coroutine void tcp_forward(int s1, int s2, int ch) {
    uint8_t c;
    while (1) {
        int err = brecv(s1, &c, 1, -1);
        if(err) break;
        err = bsend(s2, &c, 1, -1);
        if(err) break;
    }
    // send something to channel on err - close signal
    chsend(ch, &c, 1, -1);
    return;
}

coroutine void tcp_proxy(int s1, int s2) {
    int u_ch[2];
    int d_ch[2];
    int r;
    
    int err = chmake(u_ch);
    if(err) return;
    err = chmake(d_ch);
    if(err) return;
    int up = go(tcp_forward(s1, s2, u_ch[1]));
    if(up < 0) return;
    int dn = go(tcp_forward(s2, s1, d_ch[1]));
    if(dn < 0) {hclose(up); return;}
    struct chclause cc[] = {{CHRECV, u_ch[0], &r, sizeof(int)},
                           {CHRECV, d_ch[0], &r, sizeof(int)}};
    int i=choose(cc, 2, -1);
    assert(errno == 0);
    if(i == 0) {
        err = bundle_wait(up, -1);
        assert(!err);
        hclose(dn);
        err = bundle_wait(dn, -1);
    } else {
        err = bundle_wait(dn, -1);
        assert(!err);
        hclose(up);
        err = bundle_wait(up, -1);
    }
    return;
}

coroutine void tcp_echo(int s) {
    while (1) {
        uint8_t c;
        int err = brecv(s, &c, 1, -1);
        if(err) break;
        err = bsend(s, &c, 1, -1);
        if(err) break;
    }
    return;
}

coroutine void middle_segment(void) {
    struct ipaddr laddr, raddr;
    int rc = ipaddr_local(&laddr, NULL, 5678, 0);
    errno_assert(rc == 0);
    rc = ipaddr_local(&raddr, NULL, 8765, 0);
    errno_assert(rc == 0);

    int rl = tcp_listen(&raddr, 10);
    errno_assert(rl >- 0);
    int ll = tcp_listen(&laddr, 10);
    errno_assert(ll >- 0);

    int ls = tcp_accept(ll, NULL, -1);
    errno_assert(ls >= 0);
    int rs = tcp_accept(rl, NULL, -1);
    errno_assert(rs >= 0);

    tcp_proxy(ls, rs);
    tcp_close(ls, -1);
    tcp_close(rs, -1);
    return;
}

coroutine void end_segment(void) {
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, 8765, 0);
    errno_assert(rc == 0);
    
    int ls = tcp_connect(&addr, -1);
    errno_assert(ls >= 0);
    
    tcp_echo(ls);
    
    return;
}

coroutine void middle_and_end_segments(void) {
    int b = bundle();
    int err = bundle_go(b, middle_segment());
    errno_assert(!err);
    msleep(now()+250);
    err = bundle_go(b, end_segment());
    errno_assert(!err);
    err = bundle_wait(b, -1);
    return;
}

coroutine void send10k(int s) {
    for (int i = 0; i < 10000; i++) {
        int err = bsend(s, &i, sizeof(i), -1);
        errno_assert(!err);
    }
    return;
}

coroutine void recv10k(int s) {
    for (int i = 0; i < 10000; i++) {
        int err = bsend(s, &i, sizeof(i), -1);
        errno_assert(!err);
    }
    return;
}

static void tcp_concurrency_test(void) {
    int b1 = bundle();
    errno_assert(b1 >= 0);
    int err = bundle_go(b1, middle_and_end_segments());
    errno_assert(!err);
    msleep(now() + 500);
    
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, 5678, 0);
    errno_assert(rc == 0);
    int s = tcp_connect(&addr, -1);
    errno_assert(s >= 0);
    int b2 = bundle();
    errno_assert(b2 >= 0);
    err = bundle_go(b2, recv10k(s));
    errno_assert(!err);
    err = bundle_go(b2, send10k(s));
    errno_assert(!err);
    err = bundle_wait(b2, -1);
    errno_assert(!err);
    hclose(b1);
}

static void move_lots_of_data(size_t nbytes, size_t buffer_size);
static void test_fromfd();

int main(void) {
    char buf[16];

    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, 5555, 0);
    errno_assert(rc == 0);

    /* Test deadline. */
    
    struct tcp_listener_storage mem;
    int ls = tcp_listen_mem(&addr, 10, &mem);
    errno_assert(ls >= 0);
    int cr = go(client(5555));
    errno_assert(cr >= 0);
    int as = tcp_accept(ls, NULL, -1);
    errno_assert(as >= 0);
    rc = mrecv(as, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == ENOTSUP);
    rc = msend(as, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == ENOTSUP);
    int64_t deadline = now() + 30;
    ssize_t sz = sizeof(buf);
    rc = brecv(as, buf, sizeof(buf), deadline);
    errno_assert(rc == -1 && errno == ETIMEDOUT);
    int64_t diff = now() - deadline;
    time_assert(diff, 0);
    rc = brecv(as, buf, sizeof(buf), deadline);
    errno_assert(rc == -1 && errno == ECONNRESET);
    rc = hclose(as);
    errno_assert(rc == 0);
    rc = hclose(ls);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Test simple data exchange. */
    ls = tcp_listen(&addr, 10);
    errno_assert(ls >= 0);
    cr = go(client2(5555));
    errno_assert(cr >= 0);
    as = tcp_accept(ls, NULL, -1);
    errno_assert(as >= 0);
    rc = bsend(as, "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = brecv(as, buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = brecv(as, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = tcp_close(as, -1);
    errno_assert(rc == 0);
    rc = hclose(ls);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Manual termination handshake. */
    ls = tcp_listen(&addr, 10);
    errno_assert(ls >= 0);
    cr = go(client3(5555));
    errno_assert(cr >= 0);
    as = tcp_accept(ls, NULL, -1);
    errno_assert(as >= 0);
    rc = bsend(as, "ABC", 3, -1);
    errno_assert(rc == 0);
    rc = tcp_done(as, -1);
    errno_assert(rc == 0);
    rc = brecv(as, buf, 3, -1);
    errno_assert(rc == 0);
    assert(buf[0] == 'D' && buf[1] == 'E' && buf[2] == 'F');
    rc = brecv(as, buf, sizeof(buf), -1);
    errno_assert(rc == -1 && errno == EPIPE);
    rc = hclose(as);
    errno_assert(rc == 0);
    rc = hclose(ls);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);    

    /* Emulate a DoS attack. */
    ls = tcp_listen(&addr, 10);
    cr = go(client4(5555));
    errno_assert(cr >= 0);
    as = tcp_accept(ls, NULL, -1);
    errno_assert(as >= 0);
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));
    while(1) {
        rc = bsend(as, buffer, 2048, -1);
        if(rc == -1 && errno == ECONNRESET) break;
        errno_assert(rc == 0);
    }
    rc = tcp_close(as, -1);
    errno_assert(rc == -1 && errno == ECONNRESET);
    rc = hclose(ls);
    errno_assert(rc == 0);
    rc = hclose(cr);
    errno_assert(rc == 0);

    /* Test ephemeral ports. */
    rc = ipaddr_local(&addr, NULL, 0, 0);
    errno_assert(rc == 0);
    ls = tcp_listen(&addr, 10);
    errno_assert(ls >= 0);
    int port = ipaddr_port(&addr);
    assert(port > 0);
    rc = hclose(ls);
    errno_assert(rc == 0);

    /* Test whether moving lots of data through a socket pair works */
    move_lots_of_data(5000, 1);
    move_lots_of_data(5000, 1000);
    move_lots_of_data(5000, 2000);    /* This and above will succeed */
    move_lots_of_data(5000, 2001);    /* This and below will fail */
    move_lots_of_data(5000, 3000);

    test_fromfd();
    
    /* test concurrency in TCP */
    tcp_concurrency_test();

    return 0;
}

static coroutine void async_accept_routine(int listen_fd, int ch) {
    int fd = tcp_accept(listen_fd, NULL, -1);
    assert(fd != -1);
    int rc = chsend(ch, &fd, sizeof(fd), -1);
    assert(rc != -1);
    rc = hclose(ch);
    errno_assert(rc == 0);
}

static int tcp_socketpair(int fd[2]) {
    struct ipaddr server_addr;

    int rc = ipaddr_local(&server_addr, "127.0.0.1", 0, 0);
    errno_assert(rc == 0);
    int listen_fd = tcp_listen(&server_addr, 1);
    assert(listen_fd != -1);
    int port = ipaddr_port(&server_addr);
    assert(port > 0);

    int ch[2];
    rc = chmake(ch);
    errno_assert(rc == 0);
    int h = go(async_accept_routine(listen_fd, ch[0]));
    errno_assert(h >= 0);

    int client_fd = tcp_connect(&server_addr, now() + 1000);
    errno_assert(client_fd >= 0);

    int server_fd;
    rc = chrecv(ch[1], &server_fd, sizeof(server_fd), -1);
    errno_assert(rc == 0);
    assert(server_fd != -1);
    rc = hclose(h);
    errno_assert(rc == 0);
    rc = hclose(ch[1]);
    errno_assert(rc == 0);
    rc = hclose(listen_fd);
    errno_assert(rc == 0);
    
    fd[0] = client_fd;
    fd[1] = server_fd;
    return 0;
}

static void receiver(int fd, size_t nbytes, size_t buf_size, int done_ch) {
    /*
     * - static is to avoid affecting the stack.
     * - bigger than sizeof(fd_rxbuf->data) is to expose a problem.
     */
    char buf[buf_size];

    while(nbytes > 0) {
        size_t len = nbytes < sizeof(buf) ? nbytes : sizeof(buf);
        int rc = brecv(fd, buf, len, now() + 10000);
        if(rc != 0) {
            printf("While trying to receive with buffer size %zu: rc=%d, "
                   "errno=%s\n",
                   buf_size, rc, strerror(errno));
            assert(rc == 0);
        }
        nbytes -= len;
    }

    int r = chsend(done_ch, "", 1, -1);
    assert(r == 0);
}

static void move_lots_of_data(size_t nbytes, size_t buf_size) {
    int pp[2];
    int rc = tcp_socketpair(pp);
    errno_assert(rc == 0);
    int done_ch[2];
    rc = chmake(done_ch);
    errno_assert(rc == 0);
    int rcv_hdl = go(receiver(pp[0], nbytes, buf_size, done_ch[0]));
    errno_assert(rcv_hdl);

    for(size_t left = nbytes; left > 0;) {
        char buf[512];
        memset(buf, 0, sizeof(buf));
        size_t len = left < sizeof(buf) ? left : sizeof(buf);
        rc = bsend(pp[1], buf, len, now() + 1000);
        if(rc != 0) {
            printf("After moving %zu bytes rc=%d, errno=%s\n", nbytes - left,
                   rc, strerror(errno));
            assert(rc == 0);
        }
        left -= len;
    }

    char tmp;
    rc = chrecv(done_ch[1], &tmp, sizeof(tmp), -1);
    errno_assert(rc == 0);
    rc = hclose(done_ch[0]);
    errno_assert(rc == 0);
    rc = hclose(done_ch[1]);
    errno_assert(rc == 0);

    hclose(rcv_hdl);
    hclose(pp[0]);
    hclose(pp[1]);
}

static void test_fromfd() {
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, "127.0.0.1", 5555, 0);
    errno_assert(rc == 0);
    int ls = tcp_listen(&addr, 10);
    assert(ls != -1);

    int fd = socket(ipaddr_family(&addr), SOCK_STREAM, 0);
    errno_assert(fd >= 0);
    rc = connect(fd, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
    errno_assert(rc == 0);
    int s = tcp_fromfd(fd);
    errno_assert(s >= 0);
  
    int as = tcp_accept(ls, NULL, -1);
    errno_assert(s >= 0);

    rc = bsend(as, "ABC", 3, -1);
    errno_assert(rc == 0);
    char buf[3];
    rc = brecv(s, buf, 3, -1);
    errno_assert(rc == 0);

    rc = hclose(s);
    errno_assert(rc == 0);
    rc = hclose(as);
    errno_assert(rc == 0);
    rc = hclose(ls);
    errno_assert(rc == 0);
}
