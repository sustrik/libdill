# NAME

chmake - creates a channel

# SYNOPSIS

```c
#include <libdill.h>

int chmake(int chv[2]);
```

# DESCRIPTION

Creates a bidirectional channel. In case of success handles to theboth sides of the channel will be returned in **chv** parameter.

A channel is a synchronization primitive, not a container.It doesn't store any items.

**chv**: Out parameter. Two handles to the opposite ends of the channel.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine was canceled.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.

# EXAMPLE

```c
int ch[2];
int rc = chmake(ch);
if(rc == -1) {
    perror("Cannot create channel");
    exit(1);
}
```

# SEE ALSO

**chmake_mem**(3) **choose**(3) **chrecv**(3) **chsend**(3) **hclose**(3) 

