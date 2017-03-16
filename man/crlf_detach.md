# NAME

crlf_detach - terminates CRLF protocol and returns the underlying socket

# SYNOPSIS

**#include &lt;libdill.h>**

**int crlf_detach(int **_s_**, int64_t **_deadline_**);**

# DESCRIPTION

TODO

# RETURN VALUE

Underlying socket handle. On error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **EBADF**: Invalid socket handle.
* **ENOTSUP**: The handle is not a CRLF protocol handle.
* **ETIMEDOUT**: Deadline was reached.

# EXAMPLE

```c
int u = crlf_detach(s);
```
