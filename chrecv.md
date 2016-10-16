# chrecv(3) manual page

## NAME

chrecv - receive a message from a channel

## SYNOPSIS

```c
#include <libdill.h>
int chrecv(int ch, void *val, size_t len, int64_t deadline);
```

## DESCRIPTION

Retrieves a message from the channel. First parameter is the channel handle. Second points to the buffer to receive the message. Third is the size of the buffer.

The size of the buffer must match the size of elements stored in the channel, as supplied to `channel` function.

If there's no message in the channel the function waits until one becomes available or until deadline expires.

## RETURN VALUE

The function returns 0 in case of success or -1 in case of error. In the latter case it sets `errno` to one of the values below.

## ERRORS

* `EBADF`: Invalid handle.
* `ECANCELED`: Current coroutine is being shut down.
* `EINVAL`: Invalid parameter.
* `ENOTSUP`: Operation not supported. Presumably, the handle isn't a channel.
* `EPIPE`: The channel was closed using `chdone` function.
* `ETIMEDOUT`: The deadline was reached while waiting for a message.

## EXAMPLE

```c
int val;
chrecv(ch, &val, sizeof(val), now() + 1000);
```

