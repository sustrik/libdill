# NAME

chdone - mark a channel as closed

# SYNOPSIS

**#include** **&lt;libdill.h>**

**int chdone(int** _ch_**);**

# DESCRIPTION

This function is used to inform the users of a channel that it's forever closed for new messages.

After **chdone** is called on a channel, any attempts to **chsend** or **chrecv** on it will fail with an **EPIPE** error.

# RETURN VALUE

The function returns 0 on success. On error, it returns -1 and sets _errno_ to one of the following values.

# ERRORS

* **EBADF**: Not a valid handle.
* **ENOTSUP**: Operation not supported. Presumably, the handle isn't a channel.
* **EPIPE**:  **chdone** has already been called for this channel.

# EXAMPLE

```c
int result = chdone(ch);
if(result != 0) {
    perror("Channel cannot be terminated");
    exit(1);
}
printf("Channel successfully terminated.\n", val);
```

