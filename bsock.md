# bsock(3) manual page

## NAME

bsock - bytestream socket

## SYNOPSIS

```c
#include <dsock.h>
int bsend(int s, const void *buf, size_t len, int64_t deadline);
int brecv(int s, void *buf, size_t len, int64_t deadline);
int bsendv(int s, const struct iovec *iov, size_t iovlen, int64_t deadline);
int brecvv(int s, const struct iovec *iov, size_t iovlen, int64_t deadline);
```

## DESCRIPTION

Bytestream sockets (for example TCP socket or UNIX domain socket) transport a continuous stream of bytes. There are no message boundaries.

Bytestream protocols are ordered (bytes can't arrive at the destination out of order) and reliable (there are no holes in the data).

Bytestram sockets can be written to using `bsend` function. The function either writes all `len` bytes to the socket and returns 0 or fails and returns -1. In the latter case if sets `errno` to the appropriate error code. `bsendv` behaves in the same way as `bsend` except that the data is passed in a scatter array rather than in a single continuous buffer.

To read from bytestream socket use `brecv` function. The function either reads `len` bytes from the socket and returns 0 or fails and returns -1. In the latter case if sets `errno` to the appropriate error code. `brecvv` behaves in the same way as `brecv` except that the data is written to a gather array rather than into a single continuous buffer.

Bytestream send/recv functions can return following errors:

* `EBADF`: Bad file descriptor (handle).
* `ECONNRESET`: Connection to the peer broken.
* `EINVAL`: Invalid argument.
* `ENOTSUP`: Not a bytestream socket.
* `EPIPE`: Connection orderly terminated by the peer.
* `ETIMEDOUT`: Deadline expired.

## EXAMPLE

Send data to a bytestream socket:

```c
struct iovec iov[2];
iov[0].iov_base = (void*)"Hello, ";
iov[0].iov_len = 7;
iov[1].iov_base = (void*)"world!";
iov[1].iov_len = 6;
int rc = bsendv(s, iov, 2, -1);
assert(rc == 0);
```

Receive data from a bytestream socket:

```c
char buf[16];
int rc = brecv(s, buf, sizeof(buf), now() + 1000);
assert(rc == 0);
```

