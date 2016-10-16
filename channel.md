# channel(3) manual page

## NAME

channel - create a channel

## SYNOPSIS

```c
#include <libdill.h>
int channel(size_t itemsz, size_t bufsz);
```

## DESCRIPTION

Creates a channel. First parameter is the size of the items to be sent through the channel, in bytes. Second parameter is number of items that will fit into the channel. Setting this argument to zero creates an unbuffered channel.

## RETURN VALUE

Returns a channel handle. In the case of error it returns -1 and sets `errno` to one of the values below.

## ERRORS

* `ECANCELED`: Current coroutine is in the process of shutting down.
* `ENOMEM`: Not enough memory to allocate the channel.

## EXAMPLE

```c
int ch = channel(sizeof(int), 100);
```

