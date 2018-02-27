# NAME

hdone - announce end of input to a handle

# SYNOPSIS

```c
#include <libdill.h>

int hdone(int h, int64_t deadline);
```

# DESCRIPTION

 This function is used to inform the handle that there will be no
 more input. This gives it time to finish it's work and possibly
 inform the user when it is safe to close the handle.

 For example, in case of TCP protocol handle, hdone would send out
 a FIN packet. However, it does not wait until it is acknowledged
 by the peer.

 After hdone is called on a handle, any attempts to send more data
 to the handle will result in EPIPE error.

Handle implementation may also decide to prevent any further
receiving of data and return EPIPE error instead.

**h**: The handle.

**deadline**: A point in time when the operation should time out, in milliseconds. Use the **now** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EBADF**: Invalid handle.
* **ECANCELED**: Current coroutine was canceled.
* **ENOTSUP**: The handle does not support this operation.
* **EPIPE**: Pipe broken. **hdone** was already called either on this or the other side of the connection.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
http_sendrequest(s, "GET", "/" -1);
http_sendfield(s, "Host", "www.example.org", -1);
hdone(s, -1);
char reason[256];
int status = http_recvstatus(s, reason, sizeof(reason), -1);
```
