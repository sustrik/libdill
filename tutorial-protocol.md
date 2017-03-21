
# Tutorial: Implementing a network protocol

## Introduction

In this tutorial you will learn how to implement a simple network protocol using libdill.

The source code for individual steps of this tutorial can be found in `tutorial/protocol` subdirectory.

Please note that the test program as presented in this text is kept succint by ommitting all the asserts. The source code files for the tutorial, however, do the error checking properly.

## Step 1: Creating a handle

Handles are libdill's equivalent of file descriptors. A socket implementing our protocol will be pointed to by a handle. Therefore, we have to learn how to create custom handle types.

Standard include file for libdill is `libdill.h`. In this case, however, we will include `libdillimpl.h` which defines all the functions `libdill.h` does but also adds some extra stuff that can be used to implement different plugins to libdill, such as new handle types or new socket types:

```c
#include <libdillimpl.h>
```

To make it clear what API we are trying to implement, let's start with a simple test program. We'll call our protocol `quux` and at this point we will just open and close the handle:

```c
int main(void) {
    int h = quux_open();
    hclose(h);
    return 0;
}
```

To start with the implementation we need a structure to hold data for the handle. At the moment it will contain only handle's virtual function table:

```c
struct quux {
    struct hvfs hvfs;
};
```

Let's add forward declarations for functions that will be filled into the virtual function table. We'll learn what each of them is good for shortly:

```c
static void *quux_hquery(struct hvfs *hvfs, const void *id);
static void quux_hclose(struct hvfs *hvfs);
static int quux_hdone(struct hvfs *hvfs, int64_t deadline);
```

The `quux_open` function itself won't do much except for allocating the object, filling in the table of virtual functions and registering it with libdill runtime:

```c
int quux_open(void) {
    int err;
    struct quux *self = malloc(sizeof(struct quux));
    if(!self) {err = ENOMEM; goto error1;}
    self->hvfs.query = quux_hquery;
    self->hvfs.close = quux_hclose;
    self->hvfs.done = quux_hdone;
    int h = hmake(&self->hvfs);
    if(h &lt; 0) {int err = errno; goto error2;}
    return h;
error2:
    free(self);
error1:
    errno = err;
    return -1;
}
```

Function `hmake()` does the trick. You pass it a virtual function table and it returns a handle. When the standard function like `hclose()` is called on the handle libdill will forward the call to the corresponding virtual function, in this particular case to `quux_hclose()`. The interesting part is that the first argument to the virtual function is no longer the handle but rather pointer to the virtual function table. And given that virtual function table is a member of `struct quux` it's easy to convert it to the pointer to the quux object itself.

`quux_hclose()` virtual function will deallocate the quux object:

```c
static void quux_hclose(struct hvfs *hvfs) {
    struct quux *self = (struct quux*)hvfs;
    free(self);
}
```

At the moment we can just return `ENOTSUP` from the other two virtual functions.

Compile the file and run it to test whether it works as expected:

```
$ gcc -o step1 step1.c -ldill
```

## Step 2: Implementing an operation on the handle

Consider a UDP socket. It is actually multiple things. It's a handle and as such it exposes functions like `hclose()`. It's a message socket and as such it exposes functions like `msend()` and `mrecv()`. It's also a UDP socket per se and as such it exposes functions like `udp_send()` or `udp_recv()`.

libdill provides an extremely generic mechanism to address this multi-faceted nature of handles. In fact, the mechanism is so generic that it's almost silly. There's a `hquery()` function which takes ID as an argument and returns a void pointer. No assumptions are made about the nature of the ID or nature of the returned pointer. Everything is completely opaque.

To see how that can be useful let's implement a new quux function.

First, we have to define an ID for `quux` object type. Now, this may be a bit confusing, but the ID is actually a void pointer. The advantage of using a pointer as an ID is that if it was an integer you would have to worry about ID collisions, especially if you defined IDs in different libraries that were then linked together. With pointers there's no such problem. You can take a pointer of a global variable and it is guaranteed to be unique as two pieces of data can't live at the same memory location:

```c
static const int quux_id_placeholder = 0;
static const void *quux_id = &quux_id_placeholder;
```

Second, let's implement `hquery()` virtual function, empty implementation of which has been created in previous step of this tutorial:

```c
static void *quux_hquery(struct hvfs *hvfs, const void *id) {
    struct quux *self = (struct quux*)hvfs;
    if(id == quux_id) return self;
    errno = ENOTSUP;
    return NULL;
}
```

To understand what this is good for think of it from user's perspective: You call `hquery()` function and pass it a handle and the ID of quux handle type. The function will fail with `ENOTSUP` if the handle is not a quux handle. It will return pointer to `quux` structure if it is. You can use the returned pointer to perform useful work on the object.

But wait! Doesn't that break encapsulation? Anyone can call `hquery()` function, get the pointer to raw quux object and mess with it in unforeseen ways.

But no. Note that `quux_id` is defined as static. The ID is not available anywhere except in the file that implements quux handle. No external code will be able to get the raw object pointer. The encapsulation works as expected after all.

All that being said, we can finally implement our new user-facing function:

```c
int quux_frobnicate(int h) {
    struct quux *self = hquery(h, quux_type);
    if(!self) return -1;
    printf("Kilroy was here!\n");
    return 0;
}
```

Modify the test to call the new function, compile it and run it:

```c
int main(void) {
    int h = quux_open(); 
    quux_frobnicate(h);
    hclose(h);
    return 0;
}
```

## Step 3: Attaching and detaching a socket

Nice. We have a functional handle now, but in the end we want quux to be a network protocol handle. Let's say it will be a message-based protocol that can run on top of arbitrary bytestream protocol such as TCP or UNIX domain sockets.

First, we need to store the underlying socket in quux object:

```c
struct quux {
    struct hvfs hvfs;
    int u;
};
```

To accomodate libdill's naming conventions for protocols running on top of other protocols, we have to rename `quux_open()` to `quux_attach()`. We can also reuse `quux_frobnicate()` and rename it to `quux_detach()`. Attach function will accept a handle of the underlying bytestream protocol and create quux protocol on top of it. `quux_detach()` will terminate the quux protocol and return the handle of the underlying protocol:

```c
int quux_attach(int u) {
    int err;
    struct quux *self = malloc(sizeof(struct quux));
    if(!self) {err = ENOMEM; goto error1;}
    self->hvfs.query = quux_hquery;
    self->hvfs.close = quux_hclose;
    self->hvfs.done = quux_hdone;
    self->u = u;
    int h = hmake(&self->hvfs);
    if(h &lt; 0) {int err = errno; goto error2;}
    return h;
error2:
    free(self);
error1:
    errno = err;
    return -1;
}

int quux_detach(int h) {
    struct quux *self = hquery(h, quux_type);
    if(!self) return -1;
    int u = self->u;
    free(self);
    return u;
}
```

Now we can modify the test program to build two quux sockets on top of two mutually connected UNIX domain (ipc) sockets:

```c
coroutine void client(int s) {
    quux_attach(s);
    /* Do stuff here! */
    s = quux_detach(q);
    hclose(s);
}

int main(void) {
    int ss[2];
    ipc_pair(ss);
    go(client(ss[0]));
    int q = quux_attach(ss[1]);
    /* Do stuff here! */
    int s = quux_detach(q);
    hclose(s);
    return 0;
}
```

## Step 4: Sending and receiving messages

Now that we've spent previous three steps with boring scaffolding work we can finally do some fun protocol stuff.

Quux protocol is going to be message-based protocol. Therefore, we have to implement `msend()` and `mrecv()` functions. We can do that by implementing `msock` virtual function table:

```c
struct quux {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
};
```
Forward declarations for `msock` functions:

```c
static int quux_msendl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
static ssize_t quux_mrecvl(struct msock_vfs *mvfs,
    struct iolist *first, struct iolist *last, int64_t deadline);
```

Fill in the virtual function table in `quux_attach()`:

```c
int quux_attach(int u) {
    ...
    self->mvfs.msendl = quux_msendl;
    self->mvfs.mrecvl = quux_mrecvl;
    ...
}
```

Return the `msock` virtual function table when `hquery()` is called with approriate type identifier:

```c
static void *quux_hquery(struct hvfs *hvfs, const void *type) {
    struct quux *self = (struct quux*)hvfs;
    if(type == msock_type) return &self->mvfs;
    if(type == quux_type) return self;
    errno = ENOTSUP;
    return NULL;
}
```

Finally, we are going to add implementation for send and receive functions.

Note that `quux_msendl()` and `quux_mrecvl()` get pointer to to msock virtual function table. We'll have to convert that to the pointer to quux object somehow. To make that easier let's define following handy macro:

```c
#define cont(ptr, type, member) \
    ((type*)(((char*) ptr) - offsetof(type, member)))
```

The macro converts pointer to an embedded structure to a pointer of the enclosing structure. Now we can convert the `mvfs` into pointer to quux object:

```c
struct quux *self = cont(mvfs, struct quux, mvfs);
```

Let's say that quux protocol will prefix individual messages with 8-bit size. It means that messages can be at most 255 bytes long. But who cares? It's just a tutorial, after all.

To send such message we have to compute the size of the message first. We can iterate over the `iolist` (linked list of buffers, libdill's alternative to POSIX `iovec`) and sum all the buffer sizes. If size is greater than 255, tough luck. And actually, we'll use numbers 254 and 255 for special purposes later on, so limit the message size to 253 bytes:

```c
size_t sz = 0;
struct iolist *it;
for(it = first; it; it = it->iol_next)
    sz += it->iol_len;
if(sz > 253) {errno = EMSGSIZE; return -1;}
```

Now we can send the size and the payload to the underlying socket:

```c
uint8_t c = (uint8_t)sz;
int rc = bsend(self->u, &c, 1, deadline);
if(rc &lt; 0) return -1;
rc = bsendl(self->u, first, last, deadline);
if(rc &lt; 0) return -1;
```

Return zero and the send function is done.

As for receive function we'll have to read 8-bit size first:

```c
uint8_t c;
int rc = brecv(self->u, &c, 1, deadline);
if(rc &lt; 0) return -1;
```

User may pass in `NULL` instead of the buffer which means we should silently drop the message:

```c
if(!first) {
    rc = brecv(self->u, NULL, c, deadline);
    if(rc &lt; 0) return -1;
    return c;
}
```

The size of the message may not match size of the buffer supplied by the user. If message is larger than the buffer we will simply return an error. However, if message is smaller than the buffer we have to shrink the buffer to the appropriate size. We can do so by modifying the iolist. Note that iolist is not supposed be thread- or coroutine-safe and thus we can modify it as long as we revert it to its original state before returning from the function. Once the iolist is modified we can read the message payload from the underlying socket:

```c
size_t rmn = c;
struct iolist *it = first;
while(1) {
    if(it->iol_len >= rmn) break;
    rmn -= it->iol_len;
    it = it->iol_next;
    if(!it) {errno = EMSGSIZE; return -1;}
}
struct iolist orig = *it;
it->iol_len = rmn;
it->iol_next = NULL;
rc = brecvl(self->u, first, last, deadline);
*it = orig;
if(rc &lt; 0) return -1;
```

Now that we have received the message we have to return its size to the user:

```c
return c;
```

And finally, we can test whether the protocol works as expected. Let's send a simple text message (string with a terminating zero) to one end of the connection and receive it at the other end.

```c
coroutine void client(int s) {
    ...
    msend(q, "Hello, world!", 14, -1);
    ...
}

int main(void) {
    ...
    char buf[256];
    mrecv(q, buf, sizeof(buf), -1);
    printf("%s\n", buf);
    ...
}
```

Compile and test the program!

## Step 5: Error handling

Consider the following scenario: User wants to receive a message. The message is 200 bytes long. However, after reading 100 bytes, receive function times out. That puts you, as the protocol implementor, into an unconfortable position. There's no way to push the 100 bytes that were already received back to the underlying socket. libdill sockets provide no API for that, but even in principle, it would mean that the underlying socket would need an unlimited buffer. Just imagine receiving a 1TB message and timing out just before its fully read...

libdill solves this problem by not trying too hard to recover from errors, even from seemingly recoverable ones like ETIMEOUT.

When building on top of a bytestream protocol, which in unrecoverable by definition, you thus have to track failures and once error happens return an error for any subsequent attampts to receive a message. Same reasoning and same solution applies to outbound messages.

(Note that this does not apply when you are building on top of a message socket. Message sockets may be recoverable. Consider UDP. Thus, in that case you should just forward any send and received requests to the underlying socket and let it take care of error handling for you.)

Anyway, to implement error handling in quux protocol, let's add to booleans to the socket to track whether sending/receiving had failed already:

```c
struct quux {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    int senderr;
    int recverr;
};
```

Initialize them to false in `quux_attach()` function:

```c
self->senderr = 0;
self->recverr = 0;
```

Set `senderr` to true every time when sending fails and `recverr` to true every time when receiving fails. For example:

```c
if(sz > 253) {self->senderr = 1; errno = EMSGSIZE; return -1;}
```

Finally, fail send and receive function if the error flag is set:

```c
static int quux_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct quux *self = cont(mvfs, struct quux, mvfs);
    if(self->senderr) {errno = ECONNRESET; return -1;}
    ...
}
```

## Step 6: Initial handshake

Let's say we want to support mutliple versions of quux protocol. When a quux connection is established peers will exchange their version numbers and if they don't match, they will fail.

In fact, we don't even need proper handshake for that. Each peer can simply send its version number and wait for version number from the other party. We'll do this work in `quux_attach()` function. And given that sending and receiving are blocking operations `quux_attach()` will become a blocking operation itself:

```c
int quux_attach(int u, int64_t deadline) {
    ...
    const int8_t local_version = 1;
    int rc = bsend(u, &local_version, 1, deadline);
    if(rc &lt; 0) {err = errno; goto error2;}
    uint8_t remote_version;
    rc = brecv(u, &remote_version, 1, deadline);
    if(rc &lt; 0) {err = errno; goto error2;}
    if(remote_version != local_version) {err = EPROTO; goto error2;}
    ...
}
```

Note that failure of initial handshake not only prevents initialization of quux socket, it also closes the underlying socket. This is necessary because otherwise the underlying sockets will be left in undefined state, with just half of quux handshake being done.

Of course, we'll have to modify the test program to pass deadlines to `quux_attach()` function invocations.

## Step 7: Terminal handshake

Imagine that user wants to close the quux protocol and start a new protocol, say HTTP, on top of the same underlying connection. For that to work both peers would have to make sure that they've received all quux-related data before proceeding. If they did not the leftover quux data would confuse the subsequent HTTP protocol.

To achieve that peers will send a single termination byte (255) each to another to mark end of the stream of quux messages. After doing so they will read any receive and drop quux messages from the peer until they receive the 255 byte. At that point all the quux data are cleaned up, both peers have consistent view of the world and HTTP protocol can be safely initiated.

Let's add to flags to the quux socket object, one meaning "termination byte was already sent", the other "termination byte was already received":


```c
struct quux {
    ...
    int senddone;
    int recvdone;
};
```

The flags have to be initialized in the `quux_attach()` function:

```c
int quux_attach(int u, int64_t deadline) {
    ...
    self->senddone = 0;
    self->recvdone = 0;
    ...
}
```

If termination byte was already sent send function should return `EPIPE` error:

```c
static int quux_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    ...
    if(self->senddone) {errno = EPIPE; return -1;}
    ...

}
```

If termination byte was already received receive function should return `EPIPE` error. Also, we should handle the case when termination byte is originally received from the peer:

```c
static ssize_t quux_mrecvl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    ...
    if(self->recvdone) {errno = EPIPE; return -1;}
    ...
    if(c == 255) {self->recvdone = 1; errno = EPIPE; return -1;}
    ...
}
```

Virtual function `hdone()` that we've so far left unimplemented is supposed to start the terminal handshake. However, it is not supposed to wait till it is finished. The semantics of `hdone()` "user is not going to send more data". You can think of it as an EOF marker of kind.

```c
static int quux_hdone(struct hvfs *hvfs, int64_t deadline) {
    struct quux *self = (struct quux*)hvfs;
    if(self->senddone) {errno = EPIPE; return -1;}
    if(self->senderr) {errno = ECONNRESET; return -1;}
    uint8_t c = 255;
    int rc = bsend(self->u, &c, 1, deadline);
    if(rc &lt; 0) {self->senderr = 1; return -1;}
    self->senddone = 1;
    return 0;
}
```

At this point we can modify `quux_detach()` function so that it properly cleans up any leftover protocol data.

First, it will send the termination byte if it was not already sent. Then it will receive and drop messages until it receives the termination byte:

```c
int quux_detach(int h, int64_t deadline) {
    int err;
    struct quux *self = hquery(h, quux_type);
    if(!self) return -1;
    if(self->senderr || self->recverr) {err = ECONNRESET; goto error;}
    if(!self->senddone) {
        int rc = quux_hdone(&self->hvfs, deadline);
        if(rc < 0) {err = errno; goto error;}
    }
    while(1) {
        ssize_t sz = quux_mrecvl(&self->mvfs, NULL, NULL, deadline);
        if(sz < 0 && errno == EPIPE) break;
        if(sz < 0) {err = errno; goto error;}
    }
    int u = self->u;
    free(self);
    return u;
error:
    quux_hclose(&self->hvfs);
    errno = err;
    return -1;
}
```

