# NAME

fdclean - erases cached info about a file descriptor

# SYNOPSIS

```c
#include <libdill.h>
int fdclean(int fd);
```

# DESCRIPTION

This function drops any state cached by `libdill` associated with the file descriptor. It has to be called before the file descriptor is closed. If it is not, undefined behaviour may ensue.

It should also be used when you are temporarily provided with a file descriptor by a third party library, just before returning the descriptor back to the original owner.

# RETURN VALUE

Zero in case of success. In case of error -1 is returned and `errno` is set to appropriate value.

# ERRORS

* `EBUSY`: The file descriptor is being waited for using `fdin` or `fdout` at the moment.

# EXAMPLE

```c
int fds[2];
pipe(fds);
use_the_pipe(fds);
fdclean(fds[0]);
close(fds[0]);
fdclean(fds[1]);
close(fds[1]);
```

