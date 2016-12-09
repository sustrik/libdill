# NAME

channel - create a channel

# SYNOPSIS

```c
#include <libdill.h>
int channel(size_t itemsz);
```

# DESCRIPTION

Creates a channel. The parameter is the size of the items to be sent through the channel, in bytes.

The channel is a synchronizationn primitive, not a container. It doesn't store any items.

# RETURN VALUE

Returns a channel handle. In the case of error it returns -1 and sets `errno` to one of the values below.

# ERRORS

* `ECANCELED`: Current coroutine is in the process of shutting down.
* `ENOMEM`: Not enough memory to allocate the channel.

# EXAMPLE

```c
int ch = channel(sizeof(int));
if(ch == -1) {
    perror("Cannot create channel");
    exit(1);
}
```

