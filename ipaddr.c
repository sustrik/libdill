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

#include "libdill.h"
#include "utils.h"

/* Make sure that both IPv4 and IPv6 address fits into ipaddr. */
DILL_CT_ASSERT(sizeof(struct ipaddr) >= sizeof(struct sockaddr_in));
DILL_CT_ASSERT(sizeof(struct ipaddr) >= sizeof(struct sockaddr_in6));

static struct dns_resolv_conf *dill_dns_conf = NULL;
static struct dns_hosts *dill_dns_hosts = NULL;
static struct dns_hints *dill_dns_hints = NULL;

static int ipaddr_ipany(struct ipaddr *addr, int port, int mode)
{
    if(dill_slow(port < 0 || port > 0xffff)) {errno = EINVAL; return -1;}
    if (mode == 0 || mode == IPADDR_IPV4 || mode == IPADDR_PREF_IPV4) {
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
static int ipaddr_ipv4_literal(struct ipaddr *addr, const char *name,
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
static int ipaddr_ipv6_literal(struct ipaddr *addr, const char *name,
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
static int ipaddr_literal(struct ipaddr *addr, const char *name, int port,
      int mode) {
    if(dill_slow(!addr || port < 0 || port > 0xffff)) {
        errno = EINVAL;
        return -1;
    }
    int rc;
    switch(mode) {
    case IPADDR_IPV4:
        return ipaddr_ipv4_literal(addr, name, port);
    case IPADDR_IPV6:
        return ipaddr_ipv6_literal(addr, name, port);
    case 0:
    case IPADDR_PREF_IPV4:
        rc = ipaddr_ipv4_literal(addr, name, port);
        if(rc == 0)
            return 0;
        return ipaddr_ipv6_literal(addr, name, port);
    case IPADDR_PREF_IPV6:
        rc = ipaddr_ipv6_literal(addr, name, port);
        if(rc == 0)
            return 0;
        return ipaddr_ipv4_literal(addr, name, port);
    default:
        dill_assert(0);
    }
}

int ipaddr_family(const struct ipaddr *addr) {
    return ((struct sockaddr*)addr)->sa_family;
}

int ipaddr_len(const struct ipaddr *addr) {
    return ipaddr_family(addr) == AF_INET ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
}

const struct sockaddr *ipaddr_sockaddr(const struct ipaddr *addr) {
    return (const struct sockaddr*)addr;
}

int ipaddr_port(const struct ipaddr *addr) {
    return ntohs(ipaddr_family(addr) == AF_INET ?
        ((struct sockaddr_in*)addr)->sin_port :
        ((struct sockaddr_in6*)addr)->sin6_port);
}

void ipaddr_setport(struct ipaddr *addr, int port) {
    if(ipaddr_family(addr) == AF_INET)
        ((struct sockaddr_in*)addr)->sin_port = htons(port);
    else
        ((struct sockaddr_in6*)addr)->sin6_port = htons(port);
}

/* Convert IP address from network format to ASCII dot notation. */
const char *ipaddr_str(const struct ipaddr *addr, char *ipstr) {
    if(ipaddr_family(addr) == AF_INET) {
        return inet_ntop(AF_INET, &(((struct sockaddr_in*)addr)->sin_addr),
            ipstr, INET_ADDRSTRLEN);
    }
    else {
        return inet_ntop(AF_INET6, &(((struct sockaddr_in6*)addr)->sin6_addr),
            ipstr, INET6_ADDRSTRLEN);
    }
}

int ipaddr_local(struct ipaddr *addr, const char *name, int port, int mode) {
    if(!name) 
        return ipaddr_ipany(addr, port, mode);
    int rc = ipaddr_literal(addr, name, port, mode);
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
    case IPADDR_IPV4:
        ipv6 = NULL;
        break;
    case IPADDR_IPV6:
        ipv4 = NULL;
        break;
    case 0:
    case IPADDR_PREF_IPV4:
        if(ipv4)
           ipv6 = NULL;
        break;
    case IPADDR_PREF_IPV6:
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

static void dns_freeaddrinfo(struct addrinfo *ent) {
    free(ent);
}

int ipaddr_remote(struct ipaddr *addr, const char *name, int port, int mode,
      int64_t deadline) {
    int rc = ipaddr_literal(addr, name, port, mode);
    if(rc == 0)
       return 0;
    /* Load DNS config files, unless they are already chached. */
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
    /* Let's do asynchronous DNS query here. */
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
    struct dns_addrinfo *ai = dns_ai_open(name, portstr, DNS_T_A, &hints,
        resolver, &rc);
    dill_assert(ai);
    dns_res_close(resolver);
    struct addrinfo *ipv4 = NULL;
    struct addrinfo *ipv6 = NULL;
    struct addrinfo *it = NULL;
    while(1) {
        rc = dns_ai_nextent(&it, ai);
        if(rc == EAGAIN) {
            int fd = dns_ai_pollfd(ai);
            int events = dns_ai_events(ai);
            dill_assert(fd >= 0);
            int rc = (events & DNS_POLLOUT)
                        ? fdout(fd, deadline) : fdin(fd, deadline);
            /* There's no guarantee that the file descriptor will be reused
               in next iteration. We have to clean the fdwait cache here
               to be on the safe side. */
            int err = errno;
            fdclean(fd);
            errno = err;
            if(dill_slow(rc < 0)) {
                dns_ai_close(ai);
                return -1;
            }
            continue;
        }
        if(rc == ENOENT || (rc >= DNS_EBASE && rc <= DNS_ELAST))
            break;
        if(!ipv4 && it && it->ai_family == AF_INET) {
            ipv4 = it;
            it = NULL;
        }
        if(!ipv6 && it && it->ai_family == AF_INET6) {
            ipv6 = it;
            it = NULL;
        }
        dns_freeaddrinfo(it); /* Ended up useless */
        if(ipv4 && ipv6)
            break;
    }
    switch(mode) {
    case IPADDR_IPV4:
        dns_freeaddrinfo(ipv6);
        ipv6 = NULL;
        break;
    case IPADDR_IPV6:
        dns_freeaddrinfo(ipv4);
        ipv4 = NULL;
        break;
    case 0:
    case IPADDR_PREF_IPV4:
        if(ipv4) {
            dns_freeaddrinfo(ipv6);
            ipv6 = NULL;
        }
        break;
    case IPADDR_PREF_IPV6:
        if(ipv6) {
            dns_freeaddrinfo(ipv4);
            ipv4 = NULL;
        }
        break;
    default:
        dill_assert(0);
    }
    if(ipv4) {
        struct sockaddr_in *inaddr = (struct sockaddr_in*)addr;
        memcpy(inaddr, ipv4->ai_addr, sizeof (struct sockaddr_in));
        inaddr->sin_port = htons(port);
        dns_freeaddrinfo(ipv4);
        dns_freeaddrinfo(ipv6);
        dns_ai_close(ai);
        return 0;
    }
    if(ipv6) {
        struct sockaddr_in6 *inaddr = (struct sockaddr_in6*)addr;
        memcpy(inaddr, ipv6->ai_addr, sizeof (struct sockaddr_in6));
        inaddr->sin6_port = htons(port);
        dns_freeaddrinfo(ipv4);
        dns_freeaddrinfo(ipv6);
        dns_ai_close(ai);
        return 0;
    }
    dns_ai_close(ai);
    errno = EADDRNOTAVAIL;
    return -1;
}

