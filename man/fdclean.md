# NAME

fdclean - erases cached info about a file descriptor

# SYNOPSIS

```c
#include <libdill.h>
void fdclean(int fd);
```

# DESCRIPTION

This function drops any state cached by `libdill` associated with the file descriptor. It has to be called before the file descriptor is closed. If it is not, undefined behaviour may ensue.

It should also be used when you are temporarily provided with a file descriptor by a third party library, just before returning the descriptor back to the original owner.

# RETURN VALUE

None.

# ERRORS

None.

# EXAMPLE

```c
fdclean(fd);
```

