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

#if defined __linux__
#define _GNU_SOURCE
#include <netdb.h>
#include <sys/eventfd.h>
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#if !defined __sun
#include <ifaddrs.h>
#endif
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "dns/dns.h"

#define DILL_DISABLE_RAW_NAMES
#include "libdill.h"
#include "utils.h"

/* Make sure that both IPv4 and IPv6 address fits into ipaddr. */
DILL_CT_ASSERT(sizeof(struct dill_ipaddr) >= sizeof(struct sockaddr_in));
DILL_CT_ASSERT(sizeof(struct dill_ipaddr) >= sizeof(struct sockaddr_in6));

static struct dns_resolv_conf *dill_dns_conf = NULL;
static struct dns_hosts *dill_dns_hosts = NULL;
static struct dns_hints *dill_dns_hints = NULL;

static int dill_ipaddr_ipany(struct dill_ipaddr *addr, int port, int mode)
{
    if(dill_slow(port < 0 || port > 0xffff)) {errno = EINVAL; return -1;}
    if (mode == 0 || mode == DILL_IPADDR_IPV4 || mode == DILL_IPADDR_PREF_IPV4) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)addr;
        ipv4->sin_family = AF_INET;
        ipv4->sin_addr.s_addr = htonl(INADDR_ANY);
        ipv4->sin_port = htons((uint16_t)port);
#ifdef  HAVE_STRUCT_SOCKADDR_SA_LEN
        ipv4->sin_len = sizeof(struct sockaddr_in);
#endif
        return 0;
    }
    else {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)addr;
        ipv6->sin6_family = AF_INET6;
        memcpy(&ipv6->sin6_addr, &in6addr_any, sizeof(in6addr_any));
        ipv6->sin6_port = htons((uint16_t)port);
#ifdef  HAVE_STRUCT_SOCKADDR_SA_LEN
        ipv6->sin6_len = sizeof(struct sockaddr_in6);
#endif
        return 0;
    }
}

/* Convert literal IPv4 address to a binary one. */
static int dill_ipaddr_ipv4_literal(struct dill_ipaddr *addr, const char *name,
      int port) {
    struct sockaddr_in *ipv4 = (struct sockaddr_in*)addr;
    int rc = inet_pton(AF_INET, name, &ipv4->sin_addr);
    dill_assert(rc >= 0);
    if(dill_slow(rc != 1)) {errno = EINVAL; return -1;}
    ipv4->sin_family = AF_INET;
    ipv4->sin_port = htons((uint16_t)port);
#ifdef  HAVE_STRUCT_SOCKADDR_SA_LEN
    ipv4->sin_len = sizeof(struct sockaddr_in);
#endif
    return 0;
}

/* Convert literal IPv6 address to a binary one. */
static int dill_ipaddr_ipv6_literal(struct dill_ipaddr *addr, const char *name,
      int port) {
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)addr;
    int rc = inet_pton(AF_INET6, name, &ipv6->sin6_addr);
    dill_assert(rc >= 0);
    if(dill_slow(rc != 1)) {errno = EINVAL; return -1;}
    ipv6->sin6_family = AF_INET6;
    ipv6->sin6_port = htons((uint16_t)port);
#ifdef  HAVE_STRUCT_SOCKADDR_SA_LEN
    ipv6->sin6_len = sizeof(struct sockaddr_in6);
#endif
    return 0;
}

/* Convert literal IPv4 or IPv6 address to a binary one. */
static int dill_ipaddr_literal(struct dill_ipaddr *addr, const char *name, int port,
      int mode) {
    if(dill_slow(!addr || port < 0 || port > 0xffff)) {
        errno = EINVAL;
        return -1;
    }
    int rc;
    switch(mode) {
    case DILL_IPADDR_IPV4:
        return dill_ipaddr_ipv4_literal(addr, name, port);
    case DILL_IPADDR_IPV6:
        return dill_ipaddr_ipv6_literal(addr, name, port);
    case 0:
    case DILL_IPADDR_PREF_IPV4:
        rc = dill_ipaddr_ipv4_literal(addr, name, port);
        if(rc == 0)
            return 0;
        return dill_ipaddr_ipv6_literal(addr, name, port);
    case DILL_IPADDR_PREF_IPV6:
        rc = dill_ipaddr_ipv6_literal(addr, name, port);
        if(rc == 0)
            return 0;
        return dill_ipaddr_ipv4_literal(addr, name, port);
    default:
        dill_assert(0);
    }
}

int dill_ipaddr_family(const struct dill_ipaddr *addr) {
    return ((struct sockaddr*)addr)->sa_family;
}

int dill_ipaddr_len(const struct dill_ipaddr *addr) {
    return dill_ipaddr_family(addr) == AF_INET ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
}

const struct sockaddr *dill_ipaddr_sockaddr(const struct dill_ipaddr *addr) {
    return (const struct sockaddr*)addr;
}

int dill_ipaddr_port(const struct dill_ipaddr *addr) {
    return ntohs(dill_ipaddr_family(addr) == AF_INET ?
        ((struct sockaddr_in*)addr)->sin_port :
        ((struct sockaddr_in6*)addr)->sin6_port);
}

void dill_ipaddr_setport(struct dill_ipaddr *addr, int port) {
    if(dill_ipaddr_family(addr) == AF_INET)
        ((struct sockaddr_in*)addr)->sin_port = htons(port);
    else
        ((struct sockaddr_in6*)addr)->sin6_port = htons(port);
}

/* Convert IP address from network format to ASCII dot notation. */
const char *dill_ipaddr_str(const struct dill_ipaddr *addr, char *ipstr) {
    if(dill_ipaddr_family(addr) == AF_INET) {
        return inet_ntop(AF_INET, &(((struct sockaddr_in*)addr)->sin_addr),
            ipstr, INET_ADDRSTRLEN);
    }
    else {
        return inet_ntop(AF_INET6, &(((struct sockaddr_in6*)addr)->sin6_addr),
            ipstr, INET6_ADDRSTRLEN);
    }
}

int dill_ipaddr_local(struct dill_ipaddr *addr, const char *name, int port,
      int mode) {
    if(mode == 0) mode = DILL_IPADDR_PREF_IPV4;
    memset(addr, 0, sizeof(struct dill_ipaddr));
    if(!name) 
        return dill_ipaddr_ipany(addr, port, mode);
    int rc = dill_ipaddr_literal(addr, name, port, mode);
#if defined __sun
    return rc;
#else
    if(rc == 0)
       return 0;
    /* Address is not a literal. It must be an interface name then. */
    struct ifaddrs *ifaces = NULL;
    rc = getifaddrs (&ifaces);
    dill_assert (rc == 0);
    dill_assert (ifaces);
    /*  Find first IPv4 and first IPv6 address. */
    struct ifaddrs *ipv4 = NULL;
    struct ifaddrs *ipv6 = NULL;
    struct ifaddrs *it;
    for(it = ifaces; it != NULL; it = it->ifa_next) {
        if(!it->ifa_addr)
            continue;
        if(strcmp(it->ifa_name, name) != 0)
            continue;
        switch(it->ifa_addr->sa_family) {
        case AF_INET:
            dill_assert(!ipv4);
            ipv4 = it;
            break;
        case AF_INET6:
            dill_assert(!ipv6);
            ipv6 = it;
            break;
        }
        if(ipv4 && ipv6)
            break;
    }
    /* Choose the correct address family based on mode. */
    switch(mode) {
    case DILL_IPADDR_IPV4:
        ipv6 = NULL;
        break;
    case DILL_IPADDR_IPV6:
        ipv4 = NULL;
        break;
    case 0:
    case DILL_IPADDR_PREF_IPV4:
        if(ipv4)
           ipv6 = NULL;
        break;
    case DILL_IPADDR_PREF_IPV6:
        if(ipv6)
           ipv4 = NULL;
        break;
    default:
        dill_assert(0);
    }
    if(ipv4) {
        struct sockaddr_in *inaddr = (struct sockaddr_in*)addr;
        memcpy(inaddr, ipv4->ifa_addr, sizeof (struct sockaddr_in));
        inaddr->sin_port = htons(port);
        freeifaddrs(ifaces);
        return 0;
    }
    if(ipv6) {
        struct sockaddr_in6 *inaddr = (struct sockaddr_in6*)addr;
        memcpy(inaddr, ipv6->ifa_addr, sizeof (struct sockaddr_in6));
        inaddr->sin6_port = htons(port);
        freeifaddrs(ifaces);
        return 0;
    }
    freeifaddrs(ifaces);
    errno = ENODEV;
    return -1;
#endif
}

int dill_ipaddr_remote(struct dill_ipaddr *addr, const char *name, int port,
      int mode, int64_t deadline) {
    int rc = dill_ipaddr_remotes(addr, 1, name, port, mode, deadline);
    if(dill_slow(rc < 0)) return -1;
    if(rc == 0) {errno = EADDRNOTAVAIL; return -1;}
    return 0;
}

int dill_ipaddr_remotes(struct dill_ipaddr *addrs, int naddrs,
      const char *name, int port, int mode, int64_t deadline) {
    /* Check the arguments. */
    if(dill_slow(!addrs || naddrs <= 0 || !name)) {errno = EINVAL; return -1;}
    switch(mode) {
    case 0:
        mode = DILL_IPADDR_PREF_IPV4;
        break;
    case DILL_IPADDR_IPV4:
    case DILL_IPADDR_IPV6:
    case DILL_IPADDR_PREF_IPV4:
    case DILL_IPADDR_PREF_IPV6:
        break;
    default:
        errno = EINVAL;
        return -1;
    }
    /* Name may be a IP address literal. */
    memset(addrs, 0, sizeof(struct dill_ipaddr));
    int rc = dill_ipaddr_literal(addrs, name, port, mode);
    if(rc == 0) {
        if(dill_slow(mode == DILL_IPADDR_IPV6 &&
              dill_ipaddr_family(addrs) == AF_INET)) return 0;
        if(dill_slow(mode == DILL_IPADDR_IPV4 &&
              dill_ipaddr_family(addrs) == AF_INET6)) return 0;
        return 1;
    }
    /* PREF modes are done in recursive fashion. */
    if(mode == DILL_IPADDR_PREF_IPV4) {
        int rc1 = dill_ipaddr_remotes(addrs, naddrs, name, port,
            DILL_IPADDR_IPV4, deadline);
        if(dill_slow(rc1 < 0)) return -1;
        int rc2 = 0;
        if(naddrs - rc1 > 0) {
            rc2 = dill_ipaddr_remotes(addrs + rc1, naddrs - rc1, name, port,
                DILL_IPADDR_IPV6, deadline);
            if(dill_slow(rc2 < 0)) return -1;
        }
        return rc1 + rc2;
    }
    if(mode == DILL_IPADDR_PREF_IPV6) {
        int rc1 = dill_ipaddr_remotes(addrs, naddrs, name, port,
            DILL_IPADDR_IPV6, deadline);
        if(dill_slow(rc1 < 0)) return -1;
        int rc2 = 0;
        if(naddrs - rc1 > 0) {
            rc2 = dill_ipaddr_remotes(addrs + rc1, naddrs - rc1, name, port,
                DILL_IPADDR_IPV4, deadline);
            if(dill_slow(rc2 < 0)) return -1;
        }
        return rc1 + rc2;
    }
    /* We are going to do a DNS query here so load DNS config files,
       unless they are already chached. */
    if(dill_slow(!dill_dns_conf)) {
        /* TODO: Maybe re-read the configuration once in a while? */
        dill_dns_conf = dns_resconf_local(&rc);
        if(!dill_dns_conf) {
            errno = ENOENT;
            return -1;
        }
        dill_dns_hosts = dns_hosts_local(&rc);
        dill_assert(dill_dns_hosts);
        dill_dns_hints = dns_hints_local(dill_dns_conf, &rc);
        dill_assert(dill_dns_hints);
    }
    /* Launch the actual DNS query. */
    struct dns_resolver *resolver = dns_res_open(dill_dns_conf,
        dill_dns_hosts, dill_dns_hints, NULL, dns_opts(), &rc);
    if(!resolver) {
        if(errno == ENFILE || errno == EMFILE) {
            return -1;
        }
        dill_assert(resolver);
    }
    dill_assert(port >= 0 && port <= 0xffff);
    char portstr[8];
    snprintf(portstr, sizeof(portstr), "%d", port);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    struct dns_addrinfo *ai = dns_ai_open(name, portstr,
        mode == DILL_IPADDR_IPV4 ? DNS_T_A : DNS_T_AAAA, &hints,
        resolver, &rc);
    dill_assert(ai);
    dns_res_close(resolver);
    /* Wait for the results. */
    int nresults = 0;
    struct addrinfo *it = NULL;
    while(1) {
        rc = dns_ai_nextent(&it, ai);
        if(rc == EAGAIN) {
            int fd = dns_ai_pollfd(ai);
            int events = dns_ai_events(ai);
            dill_assert(fd >= 0);
            int rc = (events & DNS_POLLOUT) ?
                dill_fdout(fd, deadline) : dill_fdin(fd, deadline);
            /* There's no guarantee that the file descriptor will be reused
               in next iteration. We have to clean the fdwait cache here
               to be on the safe side. */
            int err = errno;
            dill_fdclean(fd);
            errno = err;
            if(dill_slow(rc < 0)) {
                dns_ai_close(ai);
                return -1;
            }
            continue;
        }
        if(rc == ENOENT || (rc >= DNS_EBASE && rc <= DNS_ELAST)) break;
        if(naddrs && mode == DILL_IPADDR_IPV4 && it->ai_family == AF_INET) {
            struct sockaddr_in *inaddr = (struct sockaddr_in*)addrs;
            memcpy(inaddr, it->ai_addr, sizeof (struct sockaddr_in));
            inaddr->sin_port = htons(port);
            addrs++;
            naddrs--;
            nresults++;
        }
        if(naddrs && mode == DILL_IPADDR_IPV6 && it->ai_family == AF_INET6) {
            struct sockaddr_in6 *inaddr = (struct sockaddr_in6*)addrs;
            memcpy(inaddr, it->ai_addr, sizeof (struct sockaddr_in6));
            inaddr->sin6_port = htons(port);
            addrs++;
            naddrs--;
            nresults++;
        }
        free(it);
    }
    dns_ai_close(ai);
    return nresults;
}

int dill_ipaddr_equal(const struct dill_ipaddr *addr1,
      const struct dill_ipaddr *addr2, int ignore_port) {
    if(dill_ipaddr_family(addr1) != dill_ipaddr_family(addr2)) return 0;
    switch(dill_ipaddr_family(addr1)) {
    case AF_INET:;
        struct sockaddr_in *inaddr1 = (struct sockaddr_in*)addr1;
        struct sockaddr_in *inaddr2 = (struct sockaddr_in*)addr2;
        if(inaddr1->sin_addr.s_addr != inaddr2->sin_addr.s_addr) return 0;
        if(!ignore_port && inaddr1->sin_port != inaddr2->sin_port) return 0;
        break;
    case AF_INET6:;
        struct sockaddr_in6 *in6addr1 = (struct sockaddr_in6*)addr1;
        struct sockaddr_in6 *in6addr2 = (struct sockaddr_in6*)addr2;
        if(memcmp(&in6addr1->sin6_addr.s6_addr,
              &in6addr2->sin6_addr.s6_addr,
              sizeof(in6addr1->sin6_addr.s6_addr)) != 0)
            return 0;
        if(!ignore_port && in6addr1->sin6_port != in6addr2->sin6_port) return 0;
        break;
    default:
        dill_assert(0);
    }
    return 1;
}

