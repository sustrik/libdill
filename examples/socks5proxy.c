/*

  Copyright (c) 2018 Joseph deBlaquiere

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

#include "assert.h"
#include "../libdill.h"
#include <stdio.h>
#include <string.h>

static char *auth_user = NULL;
static char *auth_pass = NULL;

int auth_fn(const char *user, const char* pass) {
    if(auth_user) {
        if(!user) return 0;
        if(strcmp(user, auth_user) != 0) return 0;
    }
    if(auth_pass) {
        if(!pass) return 0;
        if(strcmp(pass, auth_pass) != 0) return 0;
    }
    return 1;
}

// forwards input to output. signal and return on error (connection closed)
coroutine void forward(int in_s, int out_s, int ch) {
    while (1) {
        uint8_t d[1];
        if(brecv(in_s, d, 1, -1)) break;
        if(bsend(out_s, d, 1, -1)) break;
    }
    chdone(ch);
    return;
}

coroutine void do_proxy(int s) {
    if((!auth_user) || (!auth_pass)){
        if(socks5_proxy_auth(s, NULL, -1)) goto in_close;
    } else {
        if(socks5_proxy_auth(s, auth_fn, -1)) goto in_close;
    }

    struct ipaddr addr;
    int cmd = socks5_proxy_recvcommand(s, &addr, -1);
    if(cmd != SOCKS5_CONNECT) {
        socks5_proxy_sendreply(s, SOCKS5_COMMAND_NOT_SUPPORTED, &addr, -1);
        goto in_close;
    }

    int s_rem = tcp_connect(&addr, -1);
    if(s_rem < 0) goto in_close;

    if(ipaddr_remote(&addr, "0.0.0.0", 0, IPADDR_IPV4, -1)) goto both_close;

    if(socks5_proxy_sendreply(s, SOCKS5_SUCCESS, &addr, -1)) goto both_close;

    // channels for outboud, inbound to signal done (to close connection)
    int och[2], ich[2];
    if(chmake(och)) goto both_close;
    if(chmake(ich)) goto both_close;

    // start coroutines to handle inbound and outbound forwarding
    int ob = go(forward(s, s_rem, och[1]));
    if(ob < 0) goto both_close;
    int ib = go(forward(s_rem, s, ich[1]));
    if(ib < 0) goto both_close;

    // wait for message in channel - one side closed, tcp_done the other
    struct chclause cc[] = {{CHRECV, och[0], &cmd, 1},
                            {CHRECV, ich[0], &cmd, 1}};
    switch(choose(cc, 2, -1)) {
        case 0:
            if(bundle_wait(ob, -1)) break;
            tcp_done(s_rem, -1);
            bundle_wait(ib, -1);
            break;
        case 1:
            if(bundle_wait(ib, -1)) break;
            tcp_done(s, -1);
            bundle_wait(ob, -1);
            break;
        case -1:
            break;
        default:
            // unexpected answer
            assert(0);
    }
both_close:
    tcp_close(s_rem, -1);
in_close:
    tcp_close(s, -1);
    return;
}

int main(int argc, char** argv) {
    if (argc >=3) {
        // set up auth
        auth_user = argv[1];
        auth_pass = argv[2];
    }
    int workers = bundle();
    struct ipaddr addr;
    int rc = ipaddr_local(&addr, NULL, 1080, 0);
    assert(rc == 0);
    int ls = tcp_listen(&addr, 10);
    assert(ls >= 0);
    printf("SOCKS5 proxy listening on :1080\n");
    while(1) {
        struct ipaddr caddr;
        int s = tcp_accept(ls, &caddr, -1);
        assert(s >= 0);
        rc = bundle_go(workers, do_proxy(s));
        assert(rc == 0);
    }
}
