# NAME

ipc_pair - creates a pair of mutually connected IPC sockets

# SYNOPSIS

```c
#include <libdill.h>

int ipc_pair(int s[2]);
```

# DESCRIPTION

This function creates a pair of mutually connected IPC sockets.

**s**: Out parameter. Two handles to the opposite ends of the connection.

The sockets can be cleanly shut down using **ipc_close** function.

# RETURN VALUE

In case of success the function returns 0. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **ECANCELED**: Current coroutine was canceled.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOMEM**: Not enough memory.

# EXAMPLE

```c
int s[2];
int rc = ipc_pair(s);
```

# SEE ALSO

**hclose**(3) **ipc_accept**(3) **ipc_accept_mem**(3) **ipc_close**(3) **ipc_connect**(3) **ipc_connect_mem**(3) **ipc_listen**(3) **ipc_listen_mem**(3) **ipc_pair_mem**(3) 

