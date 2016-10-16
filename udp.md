# udp(3) manual page

## NAME

udp - UDP protocol

## SYNOPSIS

```c
#include <dsock.h>
int udp_socket(ipaddr *local, const ipaddr *remote);
int udp_send(int s, const ipaddr *addr, const void *buf, size_t len);
ssize_t udp_recv(int s, ipaddr *addr, void *buf, size_t len, int64_t deadline);
int udp_sendv(int s, const ipaddr *addr, const struct iovec *iov, size_t iovlen);
ssize_t udp_recvv(int s, ipaddr *addr, const struct iovec *iov, size_t iovlen,
    int64_t deadline);
```

## DESCRIPTION

TODO

## EXAMPLE

TODO

