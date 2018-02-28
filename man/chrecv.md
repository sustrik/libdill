# NAME

chrecv - receives a message from a channel

# SYNOPSIS

```c
#include <libdill.h>

int chrecv(int ch, void* val, size_t len, int64_t deadline);
```

# DESCRIPTION

Receives a message from a channel.

The size of the message requested from the channel must match the
size of the message sent to the channel. Otherwise, both peers fail
with **EMSGSIZE** error.

If there's no one sending a message at the moment, the function
blocks until someone shows up or until the deadline expires.

**ch**: The channel.

**val**: The buffer to receive the message to.

**len**: Size of the buffer, in bytes.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **EINVAL**: Invalid argument.
* **EMSGSIZE**: The peer sent a message of different size.
* **ENOTSUP**: The handle does not support this operation.
* **EPIPE**: Channel has been closed with hdone.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int val;
int rc = chrecv(ch, &val, sizeof(val), now() + 1000);
if(rc != 0) {
    perror("Cannot receive message");
    exit(1);
}
printf("Value %d received.\n", val);
```
# SEE ALSO

chmake(3) chmake_mem(3) choose(3) chsend(3) now(3) 
