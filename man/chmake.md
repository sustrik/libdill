# NAME

chmake - create a channel

# SYNOPSIS

**#include &lt;libdill.h>**

**int chmake(size_t **_itemsz_**);**

# DESCRIPTION

Creates a channel. _itemsz_ is the byte size of the items to be sent through the channel.

A channel is a synchronization primitive, not a container. It doesn't store any items.

# RETURN VALUE

Returns a channel handle. In the case of an error, it returns -1 and sets _errno_ to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine is in the process of shutting down.
* **ENOMEM**: Not enough memory to allocate a new channel.

# EXAMPLE

```c
int ch = chmake(sizeof(int));
if(ch == -1) {
    perror("Cannot create channel");
    exit(1);
}
```

