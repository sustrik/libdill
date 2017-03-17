# NAME

crlf_detach - terminates CRLF protocol and returns the underlying socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int crlf_detach(int **_s_**, int64_t **_deadline_**);**

# DESCRIPTION

CRLF is a message-based protocol that delimits messages usign CR+LF byte sequence (0x0D 0x0A). In other words, it's a protocol to send text messages separated by newlines. The protocol has no initial handshake. Terminal handshake is accomplished by each peer sending an empty line.

This function does the terminal handshake and returns underlying socket to the user. The socket is closed even in the case of error.

_deadline_ is a point in time when the operation should time out. Use the **now()** function to get your current point in time. 0 means immediate timeout, i.e., perform the operation if possible or return without blocking if not. -1 means no deadline, i.e., the call will block forever if the operation cannot be performed.

# RETURN VALUE

Underlying socket handle. On error returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **ENOTSUP**: The handle is not a CRLF protocol handle.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int u = crlf_detach(s);
```
