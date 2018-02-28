# NAME

ipc_listen_mem - starts listening for incoming IPC connections

# SYNOPSIS

```c
#include <libdill.h>

int ipc_listen_mem(const char* addr, int backlog, void* mem);
```

# DESCRIPTION

IPC  protocol is a bytestream protocol for transporting data among
processes on the same machine.  It is an equivalent to POSIX
**AF_LOCAL** sockets.

This function starts listening for incoming IPC connections.
The connections can be accepted using **ipc_accept** function.

This function allows to avoid one dynamic memory allocation by
storing the object in user-supplied memory. Unless you are
hyper-optimizing use **ipc_listen** instead.

**addr**: The filename to listen on.

**backlog**: Maximum number of connections that can be kept open without accepting them.

**mem**: The memory to store the newly created object. It must be at least **IPC_LISTENER_SIZE** bytes long and must not be deallocated before the object is closed.

The socket can be closed either by **hclose** or **ipc_close**.
Both ways are equivalent.

This function is not available if libdill is compiled with **--disable-sockets** option.

# RETURN VALUE

In case of success the function returns newly created socket. In case of error it returns -1 and sets **errno** to one of the values below.

# ERRORS

* **EACCES**: The process does not have appropriate privileges.
* **EADDRINUSE**: The specified address is already in use.
* **EINVAL**: Invalid argument.
* **ELOOP**: A loop exists in symbolic links encountered during resolution of the pathname in address.
* **EMFILE**: The maximum number of file descriptors in the process are already open.
* **ENAMETOOLONG**: A component of a pathname exceeded **NAME_MAX** characters, or an entire pathname exceeded **PATH_MAX** characters.
* **ENFILE**: The maximum number of file descriptors in the system are already open.
* **ENOENT**: A component of the pathname does not name an existing file or the pathname is an empty string.
* **ENOMEM**: Not enough memory.
* **ENOTDIR**: A component of the path prefix of the pathname in address is not a directory.

# EXAMPLE

```c
int ls = ipc_listen("/tmp/test.ipc", 10);
int s = ipc_accept(ls, -1);
bsend(s, "ABC", 3, -1);
char buf[3];
brecv(s, buf, sizeof(buf), -1);
ipc_close(s);
ipc_close(ls);
```
# SEE ALSO

hclose(3) ipc_accept(3) ipc_accept_mem(3) ipc_close(3) ipc_connect(3) ipc_connect_mem(3) ipc_listen(3) ipc_listen_mem(3) 
