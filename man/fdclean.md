# NAME

fdclean - erases cached info about a file descriptor

# SYNOPSIS

**#include &lt;libdill.h>**

**void fdclean(int **_fd_**);**


# DESCRIPTION

This function drops any state that libdill associates with file descriptor _fd_. It has to be called before the file descriptor is closed. If it is not, the behavior is undefined.

It should also be used with file descriptors provided by third-party libraries, just before returning them back to their original owners.

# RETURN VALUE

None.

# ERRORS

None.

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

