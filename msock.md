# msock(3) manual page

## NAME

msock - message socket

## SYNOPSIS

```c
#include <dsock.h>
int msend(int s, const void *buf, size_t len, int64_t deadline);
ssize_t mrecv(int s, void *buf, size_t len, int64_t deadline);
int msendv(int s, const struct iovec *iov, size_t iovlen, int64_t deadline);
ssize_t mrecvv(int s, const struct iovec *iov, size_t iovlen, int64_t deadline);
```

## DESCRIPTION

TODO

## EXAMPLE

TODO

