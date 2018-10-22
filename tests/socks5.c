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

coroutine void client(int s, char *user, char* pass) {
    int err = socks5_client_connectbyname(s, user, pass, "libdill.org", 80, -1);
    assert(err == 0);
}

coroutine void proxy(int s, char *user, char* pass) {
    // set up auth
    if(auth_user) {free(auth_user); auth_user = NULL;}
    if(auth_pass) {free(auth_pass); auth_pass = NULL;}
    if(user) {
        auth_user = malloc(strlen(user)+1); strcpy(auth_user, user);
    } else {
        auth_user = NULL;
    }
    if(pass) {
        auth_pass = malloc(strlen(pass)+1); strcpy(auth_pass, pass);
    } else {
        auth_pass = NULL;
    }

    int err;
    if((!user) || (!pass)){
        err = socks5_proxy_auth(s, NULL, -1);
    } else {
        err = socks5_proxy_auth(s, auth_fn, -1);
    }
    assert(err == 0);

    struct ipaddr addr;
    int cmd = socks5_proxy_recvcommand(s, &addr, -1);
    assert(cmd > 0);
    assert(cmd == SOCKS5_CONNECT);

    err = ipaddr_remote(&addr, "0.0.0.0", 0, IPADDR_IPV4, -1);
    assert(err == 0);

    err = socks5_proxy_sendreply(s, SOCKS5_SUCCESS, &addr, -1);
    assert(err == 0);
}

coroutine void proxy_byname(int s, char *user, char* pass) {
    // set up auth
    if(auth_user) {free(auth_user); auth_user = NULL;}
    if(auth_pass) {free(auth_pass); auth_pass = NULL;}
    if(user) {
        auth_user = malloc(strlen(user)+1); strcpy(auth_user, user);
    } else {
        auth_user = NULL;
    }
    if(pass) {
        auth_pass = malloc(strlen(pass)+1); strcpy(auth_pass, pass);
    } else {
        auth_pass = NULL;
    }

    int err;
    if((!user) || (!pass)){
        err = socks5_proxy_auth(s, NULL, -1);
    } else {
        err = socks5_proxy_auth(s, auth_fn, -1);
    }
    assert(err == 0);

    struct ipaddr addr;
    char r_name[256];
    int r_port;
    int cmd = socks5_proxy_recvcommandbyname(s, r_name, &r_port, -1);
    assert(cmd > 0);
    assert(cmd == SOCKS5_CONNECT);
    assert(strcmp(r_name, "libdill.org") == 0);
    assert(r_port == 80);

    err = ipaddr_remote(&addr, "0.0.0.0", 0, IPADDR_IPV4, -1);
    assert(err == 0);

    err = socks5_proxy_sendreply(s, SOCKS5_SUCCESS, &addr, -1);
    assert(err == 0);
}

int main(void) {
    int h[2];
    int rc = ipc_pair(h);
    assert(rc == 0);
    printf("testing IP, NO AUTH");
    int b = bundle();
    assert(b >= 0);
    rc = bundle_go(b, proxy(h[0], NULL, NULL));
    assert(rc == 0);
    rc = bundle_go(b, client(h[1], NULL, NULL));
    assert(rc == 0);
    rc = bundle_wait(b, -1);
    assert(rc == 0);
    printf("testing IP, USERNAME/PASSWORD\n");
    b = bundle();
    assert(b >= 0);
    rc = bundle_go(b, proxy(h[0], "user", "pass"));
    assert(rc == 0);
    rc = bundle_go(b, client(h[1], "user", "pass"));
    assert(rc == 0);
    rc = bundle_wait(b, -1);
    assert(rc == 0);
    printf("testing name, NO AUTH");
    rc = bundle_go(b, proxy_byname(h[0], NULL, NULL));
    assert(rc == 0);
    rc = bundle_go(b, client(h[1], NULL, NULL));
    assert(rc == 0);
    rc = bundle_wait(b, -1);
    assert(rc == 0);
    printf("testing name, USERNAME/PASSWORD\n");
    b = bundle();
    assert(b >= 0);
    rc = bundle_go(b, proxy_byname(h[0], "user", "pass"));
    assert(rc == 0);
    rc = bundle_go(b, client(h[1], "user", "pass"));
    assert(rc == 0);
    rc = bundle_wait(b, -1);
    assert(rc == 0);
    return 0;
}
