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

Message sockets (for example UDP socket or CRLF socket) transport discrete messages rather than stream of bytes. Messages are atomic. Either entire message is trasnferred or none of it. A message can be empty, i.e. zero bytes long.

Message protocols are not necessarily ordered or reliable. Messages may arrive out of order or be lost. That being said, particular message protocols may be ordered and reliable, for example, CRLF protocol layered on top of TCP protocol guarantees both reliable and ordered delivery.

Message can be sent to socket using `msend` function. The function either sends whole message and returns 0 or fails and returns -1. In the latter case if sets `errno` to the appropriate error code. `msendv` behaves in the same way as `msend` except that the data is passed in a scatter array rather than in a single continuous buffer.

To read a message from socket use `mrecv` function. If successfull, the function reads message to the supplied buffer and returns size of the message. Otherwise it returns -1 and sets `errno` to the appropriate error code. `mrecvv` behaves in the same way as `mrecv` except that the message is written to a gather array rather than into a single continuous buffer.

Message send/recv functions can return following errors:

* `EBADF`: Bad file descriptor (handle).
* `ECONNRESET`: Connection to the peer broken.
* `EINVAL`: Invalid argument.
* `EMSGSIZE`: Message larger than the protocol can transport (`msend`) or larger than the supplied buffer (`mrecv`).
* `ENOTSUP`: Not a message socket.
* `EPIPE`: Connection orderly terminated by the peer.
* `ETIMEDOUT`: Deadline expired.

## EXAMPLE

Send a message to socket:

```c
int rc = msend(s, "Boo!", 4, -1);
assert(rc == 0);
```

Receive a message from socket:

```c
char buf[256];
ssize_t msgsz = brecv(s, buf, sizeof(buf), -1);
assert(msgsz >= 0);
```


