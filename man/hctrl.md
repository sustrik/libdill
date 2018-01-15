# NAME

hctrl - coroutine's control channel

# SYNOPSIS

**#include &lt;libdill.h>**

**int hctrl;**

# DESCRIPTION

As a convenience measure, each coroutine comes with a built-in control channel. The channel has perfectly standard semantics and you can use it to communicate with the coroutine. The channel is automatically deallocated when coroutine's handle is closed.

From inside of the coroutine the control channel is accessible via _hctrl_ handle.

From outside of the coroutine you can do channel operations (e.g. _chsend_) directly on the coroutine handle. 

# EXAMPLE

```c
coroutine void worker(void) {
    chrecv(hctrl, NULL, 0, -1);
    chsend(hctrl, NULL, 0, -1);
}

int main(void) {
    /* Do a handshake with the child coroutine using zero-byte messages. */
    int h = go(worker());
    chsend(h, NULL, 0, -1);
    chrecv(h, NULL, 0, -1);
    hclose(h);
    return 0;
}

```

