# NAME

hdone - announce end of input to a handle

# SYNOPSIS

**#include** **&lt;libdill.h>**

**int hdone(int** _h_**);**

# DESCRIPTION

This function is used to inform the handle that there will be no more input. This gives it time to finish it's work and possibly inform the user when it is safe to close the handle.

After **hdone** is called on a handle, any attempts to send more data to the handle will result in **EPIPE** error.

Handle implementation may also decide to prevent any further receiving of data and return **EPIPE** error instead. 

# RETURN VALUE

The function returns 0 on success. On error, it returns -1 and sets _errno_ to one of the following values.

# ERRORS

* **EBADF**: Not a valid handle.
* **ENOTSUP**: Operation not supported.
* **EPIPE**:  Pipe broken. Possibly, **hdone** has already been called for this channel.

# EXAMPLE

```c
int rc = hdone(h);
```

