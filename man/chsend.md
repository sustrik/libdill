# NAME

chsend - send a message to a channel

# SYNOPSIS

```c
#include <libdill.h>
int chsend(int ch, const void *val, size_t len, int64_t deadline);
```

# DESCRIPTION

Send a message to the channel. First parameter is the channel handle. Second points to the buffer that contains the message to send. Third is the size of the buffer.

The size of the buffer must match the size of elements stored in the channel, as supplied to `channel` function.

If there's no space in the channel for the message the function waits until it becomes available or until deadline expires.

# RETURN VALUE

The function returns 0 in case of success or -1 in case of error. In the latter case it sets `errno` to one of the values below.

# ERRORS

* `EBADF`: Invalid handle.
* `ECANCELED`: Current coroutine is being shut down.
* `EINVAL`: Invalid parameter.
* `ENOTSUP`: Operation not supported. Presumably, the handle isn't a channel.
* `EPIPE`: The channel was closed using `chdone` function.
* `ETIMEDOUT`: The deadline was reached while waiting for a message.

# EXAMPLE

```c
int val = 42;
chsend(ch, &val, sizeof(val), now() + 1000);
```

