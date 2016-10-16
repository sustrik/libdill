# bsock(3) manual page

## NAME

bsock - bytestream socket

## SYNOPSIS

```
#include <dsock.h>
int bsend(int s, const void *buf, size_t len, int64_t deadline);
int brecv(int s, void *buf, size_t len, int64_t deadline);
int bsendv(int s, const struct iovec *iov, size_t iovlen, int64_t deadline);
int brecvv(int s, const struct iovec *iov, size_t iovlen, int64_t deadline);
```

## DESCRIPTION

TODO

## EXAMPLE

TODO

