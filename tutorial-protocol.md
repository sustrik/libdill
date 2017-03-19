
# Tutorial: Implementing a network protocol

## Introduction

In this tutorial you will learn how to implement a simple network protocol using libdill.

Given that tutorial is supposed to demonstrate different aspects of the problem the protocol will be relatively complex. In the real world you would want to split it into multiple micro-protocols.

Note that the test program as presented in this text is kept succint by ommitting all the asserts. The source code files for the tutorial, however, do the error checking properly.

The source code for individual steps of this tutorial can be found in `tutorial/protocol` subdirectory.

## Step 1: Creating a handle

Handles are libdill's equivalent of file descriptors. An instance of our protocol will be pointed to by a socket which is a type of handle. Therefore, we have to learn how to create custom handle types.

First of all, we have to include `libdillimpl.h` header file. It in turn includes standard `libdill.h` but also adds extra functions used for implementing plugins to libdill.

Let's say out new handle type will be called `quux`. Now we'll add some testing code. It will do nothing but open and close the handle:

```c
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <libdillimpl.h>

int main(void) {
    int h = quux_open();
    hclose(h);
    return 0;
}
```

We'll need a structure to hold data for our handle. At the moment it will be empty, except for the table of handle's virtual functions:

```c
struct quux {
    struct hvfs hvfs;
};
```

Let's add forward declarations for functions to be filled into our virtual function table. We'll learn what they are good for shortly.

```c
static void *quux_hquery(struct hvfs *hvfs, const void *type);
static void quux_hclose(struct hvfs *hvfs);
static int quux_hdone(struct hvfs *hvfs, int64_t deadline);
```

The `quux_open` function itself doesn't do much except for allocating the object, filling in the table of virtual functions and letting libdill know about the handle:

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

Now we can implement the virtual functions. At the moment we can jusr return `ENOTSUP` from `quux_hquery` and `quux_hdone`. As for `quux_hclose` it will be called when user tries to close the handle using `hdone()` function. We can fill in the code to deallocate our object. Note that we are passed pointer to the virtual function table that, given that it's first member in the structure, can be simply cast to `struct quux`:

```c
static void quux_hclose(struct hvfs *hvfs) {
    struct quux *self = (struct quux*)hvfs;
    free(self);
}
```

Compile the file and run it to test whether it works as expected:

```
$ gcc -o step1 step1.c -ldill
```

## Step 2: Implementing an operation on the handle

Consider a UDP socket. It is actually multiple things. It's a handle and as such it exposes functions like `hclose()`. It's a message socket and as such it exposes functions like `msend()` and `mrecv()`. It's also a UDP socket per se and as such it exposes functions like `udp_send()` or `udp_recv()`.

To address this multi-faceted nature of handles libdill provides a mechanism map type identifiers to pointers. Function `hquery()` gets a handle and a type identifier and returns a void pointer. It makes no assumptions about what the resulting pointer points to.

To see how this could be useful let's implement a new function on `quux` handle.

First, we have to define a type identifier for `quux` objects. Type identifier is a unique constant void pointer and can be easily generated like this:

```c
static const int quux_type_placeholder = 0;
static const void *quux_type = &quux_type_placeholder;
```

Note that `quux_typeid` must be unique within the process because no other int can live at the same memory address.

Second, let's implement `quux_hquery` virtual function, empty implementation of which we have created in previous step of this tutorial:

```c
static void *quux_hquery(struct hvfs *hvfs, const void *type) {
    struct quux *self = (struct quux*)hvfs;
    if(type == quux_type) return self;
    errno = ENOTSUP;
    return NULL;
}
```

Finally, we can implement our new user-facing function:

```c
int quux_frobnicate(int h) {
    struct quux *self = hquery(h, quux_type);
    if(!self) return -1;
    /* Frobnicate the object here! */
    return 0;
}
```

Now we can call the new function from the test code:

```c
int main(void) {
    int h = quux_open(); 
    quux_frobnicate(h);
    hclose(h);
    return 0;
}
```

As can be seen, `hquery()` is used to translate the handle to the pointer to the object.

The mechanism also offers type safety. Try calling `quux_frobnicate()` with channel or a coroutine handle and you'll get `ENOTSUP` error. This happens because channel's or coroutine's virtual query function know nothing about `quux_type` and so it fails with `ENOTSUP` error.

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
int main(void) {
    int ss[2];
    ipc_pair(ss);
    int q1 = quux_attach(ss[0]);
    int q2 = quux_attach(ss[1]);
    /* Do something useful here! */
    int s = quux_detach(q2);
    hclose(s);
    s = quux_detach(q1);
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

And finally, we can test whether the protocol works as expected. Let's send a simple text message (string with a terminating zero) to one end of the connection and receive it at the other end:

```c
msend(q1, "Hello, world!", 14, -1);
char buf[256];
mrecv(q2, buf, sizeof(buf), -1);
printf("%s\n", buf);
```

Compile and test the program!

# Step 5: Error handling

Consider the following scenario: User asks to receive a message. The message is 200 bytes long. However, after reading 100 bytes, receive function times out. That puts you, as the protocol implementor, into an unconfortable position. There's no way to push the 100 bytes that were already received back to the underlying socket. libdill sockets provide no API for that, but even in principle, it would mean that the underlying socket would need an unlimited buffer. Just imagine receiving a 1TB message and timing out just before its fully read...

libdill solves this problem by not trying too hard to recover from errors, even from seemingly recoverable ones like ETIMEOUT.

When building on top of a bytestream protocol, which in unrecoverable by definition, you thus have to track failures and once error happens return an error for any subsequent attampts to receive a message. Same reasoning and same solution applies to outbound messages.

(Note that this does not apply when you are building on top of a message socket. Message sockets may be recoverable. Just think of UDP. Thus, in that case you should just forward and send and received requests to the underlying socket and let it take care of error handling for you.)

Anyway, to implement error handling in quux protocol, let's add to booleans to the socket to track whether sending/receiving had failed already:

```
struct quux {
    struct hvfs hvfs;
    struct msock_vfs mvfs;
    int u;
    int senderr;
    int recverr;
};
```

Initialize them to false in `quux_attach()` function:

```
self->senderr = 0;
self->recverr = 0;
```

Set `senderr` to true every time when sending fails and `recverr` to true every time when receiving fails. For example:

```
if(sz > 253) {self->senderr = 1; errno = EMSGSIZE; return -1;}
```

Finally, fail send and receive function if the error flag is set:

```
static int quux_msendl(struct msock_vfs *mvfs,
      struct iolist *first, struct iolist *last, int64_t deadline) {
    struct quux *self = cont(mvfs, struct quux, mvfs);
    if(self->senderr) {errno = ECONNRESET; return -1;}
    ...
}
```
