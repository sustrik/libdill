# NAME

pfx_detach - terminates PFX protocol and returns the underlying socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int pfx_detach(int **_s_**, int64_t **_deadline_**);**

# DESCRIPTION

PFX is a message-based protocol to send binary messages prefixed by 8-byte size in network byte order. The protocol has no initial handshake. Terminal handshake is accomplished by each peer sending eight 0xFF bytes.

This function does the terminal handshake and returns underlying socket to the user. The socket is closed even in the case of error.

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

Underlying socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **ENOTSUP**: The handle is not a PFX protocol handle.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int u = pfx_detach(s);
```
