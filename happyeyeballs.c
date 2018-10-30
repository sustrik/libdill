/*

  Copyright (c) 2018 Martin Sustrik

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

#include <errno.h>

#define DILL_DISABLE_RAW_NAMES
#include "libdill.h"
#include "utils.h"

static dill_coroutine void dill_happyeyeballs_dnsquery(const char *name,
      int port, int mode, int ch) {
    /* Do the DNS query. Let's be reasonable and limit the number of addresses
       to 10 per IP version. */
    struct dill_ipaddr addrs[10];
    int count = dill_ipaddr_remotes(addrs, 10, name, port, mode, -1);
    if(count > 0) {
        /* Send the results of the query to the main fuction. */
        int i;
        for(i = 0; i != count; ++i) {
           int rc = dill_chsend(ch, &addrs[i], sizeof(struct dill_ipaddr), -1);
           if(dill_slow(rc < 0 && errno == ECANCELED)) return;
           dill_assert(rc == 0);
        }
    }
    /* chdone is deliberately not being called here so that sorter doesn't
       have to deal with closed channels. Instead we'll send 0.0.0.0 address
       to mark the end of the results. */
    struct dill_ipaddr addr;
    int rc = dill_ipaddr_local(&addr, "0.0.0.0", 0, DILL_IPADDR_IPV4);
    dill_assert(rc == 0);
    rc = dill_chsend(ch, &addr, sizeof(struct dill_ipaddr), -1);
    dill_assert(rc == 0 || errno == ECANCELED);
}

static dill_coroutine void dill_happyeyeballs_attempt(struct dill_ipaddr addr,
      int ch) {
    int conn = dill_tcp_connect(&addr, -1);
    if(dill_slow(conn < 0)) return;
    int rc = dill_chsend(ch, &conn, sizeof(int), -1);
    if(dill_slow(rc < 0 && errno == ECANCELED)) {
        rc = dill_hclose(conn);
        dill_assert(rc == 0);
        return;
    }
    dill_assert(rc == 0);
}

static dill_coroutine void dill_happyeyeballs_coordinator(
      const char *name, int port, int ch) {
    struct dill_ipaddr nulladdr;
    int rc = dill_ipaddr_local(&nulladdr, "0.0.0.0", 0, DILL_IPADDR_IPV4);
    dill_assert(rc == 0);
    /* According to the RFC, IPv4 and IPv6 DNS queries should be done in
       parallel. Create two coroutines and two channels to pass
       the addresses from them. */
    int chipv6[2];
    struct dill_chstorage chipv6_storage;
    rc = dill_chmake_mem(&chipv6_storage, chipv6);
    dill_assert(rc == 0);
    int chipv4[2];
    struct dill_chstorage chipv4_storage;
    rc = dill_chmake_mem(&chipv4_storage, chipv4);
    dill_assert(rc == 0);
    struct dill_bundle_storage bndl_storage;
    int bndl = dill_bundle_mem(&bndl_storage);
    dill_assert(bndl >= 0);
    rc = dill_bundle_go(bndl, dill_happyeyeballs_dnsquery(name, port,
        DILL_IPADDR_IPV6, chipv6[1]));
    if(dill_slow(rc < 0 && errno == ECANCELED)) goto cancel;
    dill_assert(rc == 0);
    rc = dill_bundle_go(bndl, dill_happyeyeballs_dnsquery(name, port,
        DILL_IPADDR_IPV4, chipv4[1]));
    if(dill_slow(rc < 0 && errno == ECANCELED)) goto cancel;
    dill_assert(rc == 0);
    /* RFC says to wait for 50ms for IPv6 result irrespective of whether
       IPv4 address arrives. */
    struct dill_ipaddr addr;
    struct dill_chclause cls[2] = {
        {DILL_CHRECV, chipv6[0], &addr, sizeof(struct dill_ipaddr)},
        {DILL_CHRECV, chipv4[0], &addr, sizeof(struct dill_ipaddr)}
    };
    rc = dill_chrecv(chipv6[0], &addr, sizeof(struct dill_ipaddr),
        dill_now() + 50);
    if(dill_slow(rc < 0 && errno == ECANCELED)) goto cancel;
    int idx;
    int64_t nw;
    if(dill_fast(rc == 0)) {
        idx = 0;
        nw = dill_now();
        goto use_address;
    }
    dill_assert(errno == ETIMEDOUT);
    /* From now we can use any address. */
    while(1) {
        nw = dill_now();
        idx = dill_choose(cls, 2, -1);
        if(dill_slow(idx < 0 && errno == ECANCELED)) goto cancel;
        dill_assert(idx >= 0 && errno == 0);
use_address:
        /* Ignore 0.0.0.0 addresses. */
        if(dill_ipaddr_equal(&addr, &nulladdr, 0)) continue;
        /* Launch the connect attempt. */
        rc = dill_bundle_go(bndl, dill_happyeyeballs_attempt(addr, ch));
        if(dill_slow(rc < 0 && errno == ECANCELED)) goto cancel;
        dill_assert(rc == 0);
        /* Alternate between IPv4 and IPv6 addresses. */
        if(idx == 0) {
            int tmp = cls[0].ch;
            cls[0].ch = cls[1].ch;
            cls[1].ch = tmp;
        }
        /* Wait for 300ms before launching next connection attempt. */
        rc = dill_msleep(nw + 300);
        if(dill_slow(rc < 0 && errno == ECANCELED)) goto cancel;
        dill_assert(rc == 0);
    }
cancel:
    rc = dill_hclose(bndl);
    dill_assert(rc == 0);
    rc = dill_hclose(chipv4[0]);
    dill_assert(rc == 0);
    rc = dill_hclose(chipv4[1]);
    dill_assert(rc == 0);
    rc = dill_hclose(chipv6[0]);
    dill_assert(rc == 0);
    rc = dill_hclose(chipv6[1]);
    dill_assert(rc == 0);
}

int dill_happyeyeballs_connect(const char *name, int port, int64_t deadline) {
    int res = -1;
    int err = 0;
    if(dill_slow(!name || port <= 0)) {err = EINVAL; goto exit1;}
    int chconns[2];
    struct dill_chstorage chconns_storage;
    int rc = dill_chmake_mem(&chconns_storage, chconns);
    if(dill_slow(rc < 0)) {err = errno; goto exit1;}
    int coord = dill_go(dill_happyeyeballs_coordinator(name, port, chconns[1]));
    if(dill_slow(coord < 0)) {err = errno; goto exit2;}
    int conn;
    rc = dill_chrecv(chconns[0], &conn, sizeof(int), deadline);
    if(dill_slow(rc < 0)) {err = errno; goto exit3;}
    res = conn;
exit3:
    rc = dill_hclose(coord);
    dill_assert(rc == 0);
exit2:
    rc = dill_hclose(chconns[0]);
    dill_assert(rc == 0);
    rc = dill_hclose(chconns[1]);
    dill_assert(rc == 0);
exit1:
    errno = err;
    return res;
}

