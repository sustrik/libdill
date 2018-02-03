# NAME

chmake - create a channel

# SYNOPSIS

**#include &lt;libdill.h>**

**int chmake(int **_chv_**[2]);**

# DESCRIPTION

Creates a bidirectional channel. In case of success handles to the both sides of the channel will be returned in _chv_ parameter. 

A channel is a synchronization primitive, not a container. It doesn't store any items.

# RETURN VALUE

Returns a channel handle. In the case of an error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ENOMEM**: Not enough memory to allocate a new channel.

# EXAMPLE

```c
int ch[2];
int rc = chmake(ch);
if(rc == -1) {
    perror("Cannot create channel");
    exit(1);
}
```

