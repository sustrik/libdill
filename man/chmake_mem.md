# NAME

chmake_mem - creates a channel

# SYNOPSIS

```c
#include <libdill.h>

int chmake_mem(void* mem, int chv[2]);
```

# DESCRIPTION

Creates a bidirectional channel. In case of success handles to the
both sides of the channel will be returned in **chv** parameter.

A channel is a synchronization primitive, not a container.
It doesn't store any items.

This function allows to avoid one dynamic memory allocation by
storing the object in user-supplied memory. Unless you are
hyper-optimizing use **chmake** instead.

**mem**: The memory to store the newly created object. It must be at least **CHSIZE** bytes long and must not be deallocated before the object is closed.

**chv**: Out parameter. Two handles to the opposite ends of the channel.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine was canceled.
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

chmake(3) choose(3) chrecv(3) chsend(3) 
