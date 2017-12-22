# NAME

chrecv - receive a message from a channel

# SYNOPSIS


**#include &lt;libdill.h>**

**int chrecv(int **_ch_**, void **\*_val_**, size_t** _len_**, int64_t** _deadline_**);**

# DESCRIPTION

Retrieves a message from a channel. The _ch_ parameter is the channel handle. _val_ points to a buffer to receive the message to. _len_ is the size of the buffer.

The size of the buffer must match the size of the elements stored in the channel as supplied to the **chmake** function.

If no sender is available, the function waits until one arrives or until the deadline expires.

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

The function returns 0 on success. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine is being shut down.
* **EINVAL**: Invalid parameter.
* **ENOTSUP**: Operation not supported. Presumably, the handle isn't a channel.
* **EPIPE**: Channel has been closed with **hdone**.
* **ETIMEDOUT**: Deadline expired while waiting on a message.

# EXAMPLE

```c
int val;
int result = chrecv(ch, &val, sizeof(val), now() + 1000);
if(result != 0) {
    perror("Cannot receive message");
    exit(1);
}
printf("Value %d received.\n", val);
```

