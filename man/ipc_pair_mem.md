# NAME

ipc_pair_mem - creates a pair of mutually connected IPC sockets

# SYNOPSIS

```c
#include <libdill.h>

int ipc_pair_mem(void* mem, int s[2]);
```

# DESCRIPTION

This function creates a pair of mutually connected IPC sockets.

This function allows to avoid one dynamic memory allocation by
storing the object in user-supplied memory. Unless you are
hyper-optimizing use **ipc_pair** instead.

**mem**: The memory to store the newly created object. It must be at least **IPC_PAIR_SIZE** bytes long and must not be deallocated before the object is closed.

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

**hclose**(3) **ipc_accept**(3) **ipc_accept_mem**(3) **ipc_close**(3) **ipc_connect**(3) **ipc_connect_mem**(3) **ipc_listen**(3) **ipc_listen_mem**(3) **ipc_pair**(3) 
