# ﻿BSD Socket API Revamp

## Abstract

This memo describes new API for network sockets. Compared to classic BSD socket API the new API is much more lightweight and flexible. Its primary focus is on easy composability of network protocols.

## Introduction

The area of network protocols is seeing little innovation. This stems from wide range of obstacles the network protocol implementors are facing. The main one, though, is almost zero reusability of the functionality that already exists.

This memo proposes to fix the reusability problem by revamping the old BSD socket API and while doing so strongly focusing on composability of protocols.

This document doesn’t provide rationale for individual design choices. The rationale is provided in a separate informational RFC 0000 [1].

## Terminology

### Keywords

The keywords "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT",  "SHOULD", "SHOULD NOT", "RECOMMENDED",  "MAY", and "OPTIONAL" in this document are to be interpreted as described in RFC 2119.

### Base protocols

If protocol lives at the bottom of the stack it is called “base” protocol.

However, being a base protocol is an API concept. As long as the underlying protocol is not exposed via the API, the protocol is considered to be a base protocol.

API proposed in this memo requires base protocols to be initialized using functions such as “open”, “connect” or “accept” and to be terminated via “close” function.

Being a base protocol is mutually exclusive with being an overlay protocol.

### Overlay protocols

Protocol that is layered on top of another protocol is called “overlay” protocol.

API proposed in this memo requires overlay protocols to be initialized using “attach” function and to be terminated via “detach” function.

Being an overlay protocol is mutually exclusive with being a base protocol.

### Transport protocols

Transport protocols are protocols capable of sending and/or receiving unstructured binary data, whether in form of bytes or messages. Examples of transport protocols are IP, TCP, UDP, SCTP. WebSockets and so on.

API proposed in this memo requires transport protocols to implement functions such as “bsendl”, “brecvl”, “msendl” or “mrecvl”.

Being a transport protocol is mutually exclusive with being an application protocol.

### Application protocols

Application protocols, instead of sending and receiving raw data, provide user with a way to accomplish specific tasks. For example, DNS protocol provides a way to resolve names. Other examples of application protocols are LDAP, DHCP, SNMP, BGP and so on.

This specification doesn’t concern itself with normal operation of application protocols. Still, application protocols MUST follow this specification when it comes to protocol initialization and termination.

Application protocol always lives on the top of the stack. Being an application protocol is mutually exclusive with being a transport protocol.

### Bytestream protocols

Bytestream protocols are transport protocols that don’t define message boundaries. While the data are necessarily sent and received in chunks, the boundaries of these chunks are not guaranteed to be preserved as they pass from the sender to the receiver.

TCP is classic example of a bytestream protocol.

Bytestream protocols are, by their nature, reliable (no data can be dropped), ordered (data arrive in the same order as they were sent it) and non-recoverable (once an error is encountered, there’s no way to re-attach to the stream of bytes).

This memo requires bytestream protocols to implement functions such as “bsendl” and “brecvl”.

Being a bytestream protocol is mutually exclusive with being a message protocol.

### Message protocols

Message protocols are transport protocols that preserve message boundaries.  Examples of message protocols are IP, UDP, SCTP, PGM, WebSockets and so on.

Message protocols are not required to be reliable (messages may be dropped) or ordered (messages may be delivered in the same order they were sent in). However, they are required to be atomic: Either full message is received or no message is. The messages are never truncated or re-fragmented.

Message protocols may or may not be able to recover after an error.

This memo requires message protocols to implement functions such as “msendl” and “mrecvl”.

Being a message protocol is mutually exclusive with being a bytestream protocol.

## Protocol naming conventions

Protocol names MUST be in lowercase. If multiple words are needed they MUST be separated by underscores.

Given that official protocol names are often quite long (e.g. “Datagram Congestion Control Protocol”), it is RECOMMENDED to prefer abbreviations (e.g. “dccp”) to full names. 

Whenever possible, the protocol name, as used in the API, SHOULD correspond to the name of the protocol, not to the name of the protocol implementation.

Often, there’s a naming conflict between “full” and “raw” version of the protocol. For example, with WebSockets one may consider the whole diagram below to be “WebSockets protocol”. However, it is also reasonable to refer to the box on top right, without the underlying TCP connection and initial HTTP handshake as “WebSockets protocol”.

```
+-------------------------+
| HTTP |                  |
+------+    WebSockets    |
| CRLF |                  |
+-------------------------+
|          TCP            |
+-------------------------+
```

In these cases the end user’s perspective SHOULD be taken into account when making the call.

In the case of WebSockets users typically want to instantiate the entire stack, including TCP and HTTP in one go, while using the raw version of the protocol (e.g. to layer it on top of UNIX sockets) is more of a specialist use case. Therefore, “ws” protocol name should be used for the full version of the protocol, while the raw version should use something less shiny, like “ws_raw”.

## Function naming conventions

The function names MUST be composed of short protocol name and action name separated by an underscore (e.g. "tcp_connect").

## Handles

Handle is an integer referring to a protocol instance, very much like file descriptor. In fact, handles MAY map 1:1 to file descriptors. However, POSIX provides no way to create custom file descriptor types and it is thus hard to use true file descriptors in the user space. Therefore, this specification will use term “handle” and will remain agnostic about relationship between handles and file descriptors.

Handles MUST NOT be negative.

Following generic handle-related functions should be available:

```
int hdone(int h, int64_t deadline);
int hclose(int h);
```

Function hdone lets the handle know that no more data will be written to it. Any subsequent attempts to write data MUST result in EPIPE error. The function MAY be blocking.

Function hclose closes the handle. It MUST NOT perform any blocking operation such as flushing buffered data to the network or doing a handshake with the peer.

## Deadlines

All functions that can possibly block MUST have a deadline parameter. The deadline SHOULD be the last parameter of the function.

Unlike with BSD sockets the deadlines are points in time rather than intervals.

Deadlines are 64-bit signed integers constructed using now function:

```
int64_t now(void);
```

The function returns current time in milliseconds. The result MUST NOT be negative or zero. The underlying clock MUST be monotonic.

For example, to receive a message with a deadline 1 second from now one may do the following:

```
mrecv(s, buf, sizeof(buf), now() + 1000);
```

Negative and zero deadlines have special meaning. Zero deadline means: “Perform the operation if you can do so without blocking. Time out otherwise.” Negative number means: “Never time out.”

When function times out it MUST return ETIMEDOUT error.

## Protocol initialization

A base protocol is initialized using a protocol-specific start functions. Protocols vary wildly with respect to how they are initialized. Therefore, this memo poses no requirements on start functions. Good names for start functions are, for example, “open” (udp_open), “listen” (tcp_listen), “connect” (ws_connect), “accept” (tcp_accept) and so on.

An overlay protocol SHOULD be initialized via “attach” function (e.g. crlf_attach). The first argument to the attach function should be the handle of the underlying protocol.

It is RECOMMENDED that the implementation of the attach function causes the underlying protocol handle passed to it to be no longer usable after the function returns. This can be done, for example, by duplicating the handle, keeping the duplicate and closing the original handle.

Both base protocol start functions and overlay protocol attach functions MAY have arbitrary number of additional arguments. They MUST return a newly created socket handle.

In case of error the function MUST return -1 and set errno to appropriate value. They MUST also close the underlying handle. This is, of course, not possible if the handle was invalid and EBADF error was returned.

If protocol requires an initial handshake it MUST be performed in this phase of the socket lifecycle. Initialization function MUST NOT return until the handshake is finished.

If protocol initialization can be blocking these functions MUST accept deadline parameter and time out when deadline is reached.

## Protocol termination

Protocol termination comes in two flavours. There’s a forceful termination and an orderly termination.

### Forceful termination

Forceful termination means that the user wants to shut down the socket abruptly and without blocking.

To perform forceful termination hclose function is used. The protocol MUST shut down immediately. It MUST NOT flush buffered outbound data or do terminal handshake with the peer if that would result in blocking.

That being said, it is reasonable to at least to try to play nice with the peer. For example, TCP implementation may try to send a RST packet if it is able to do so without blocking.

The protocol MUST clean up all resources it owns including closing the underlying protocol handle. Given that the underlying protocol does the same operation, an entire stack of protocols can be shut down recursively by closing the file descriptor of the topmost protocol:

```
int h1 = foo_open();
int h2 = bar_attach(h1);
int h3 = baz_attach(h2);
hclose(h3); /* baz, bar and foo are shut down */
```

In case of success hclose returns zero. In case of error it returns -1 and sets errno to the appropriate value. The resources owned by the handle MUST be released even in the case of error.

### Orderly termination

Orderly termination means that the terminal handshake with the peer, if required by the protocol, is performed. Orderly termination leaves both peers with a consistent view of the world. It is thus possible to continue using the underlying protocol after orderly termination of the layover protocol is finished.

To perform an orderly shutdown of a base protocol there SHOULD be a protocol-specific function called "close" (e.g. "tcp_close").

To orderly shut down an overlay protocol there SHOULD be a protocol specific “detach” function (e.g. “crlf_detach”).

In addition to the handle to shut down the functions can have arbitrary number of other arguments. For example, one such argument may be a "shutdown reason" string to be sent to the peer.

If orderly shut down is a blocking operation the last parameter MUST be a deadline.

Function “close” MUST return zero in case of success. Function “detach” MUST return the handle of the underlying protocol in case of success. In case of error both functions MUST forcefully close the socket (including all underlying sockets), return -1 and set errno to the appropriate value.

The user is allowed to split orderly termination into two steps. First, the outbound half of the connection can be closed using hdone function. That initiates the terminal handshake. However, the user is still able to receive data from the peer. After all the remaining data are read the socket can be closed using close or detach function, as appropriate.

Example of user code doing two-step termination:

```
hdone(s);
while(1) {
    int rc = mrecv(s, &msg, sizeof(msg), -1);
    if(rc < 0 && errno == EPIPE) break;
    process_msg(&msg); 
}
hclose(s);
```

Given that protocol termination is tricky it will be defined using pseudocode.

To get the trivial case out of the way: If the protocol is a base protocol that doesn’t support termination handshakes, it returns an error from hdone function and its close function is equivalent to raw hclose function:

```
int foo_done(int h, int64_t deadline) {
    errno = ENOTSUP;
    return -1;
}


int foo_close(int h) {
    return hclose(h);
}
```

For all other cases, there are two ways to implement hdone function. Either the protocol doesn’t define the terminal handshake, in which case hdone just forwards the call to the underlying protocol:

```
int foo_done(int h, int64_t deadline) {
    struct foo_object *self = foo_data(h);
    return hdone(self->underlying);
}
```

Or the protocol does define the terminal handshake in which case the handshake must be started in hdone function:

```
int foo_done(int h, int64_t deadline) {
    struct foo_object *self = foo_data(h);
    if(self->done) {errno = EPIPE; return -1;}
    int rc = foo_start_terminal_handshake(h, deadline);
    if(rc < 0) return -1;
    self->done = 1;
    return 0;
}
```

For base protocols there is also close function which starts the termination handshake if it was not started yet, reads all the remaining data and closes the socket:

```
int foo_close(int h, int64_t deadline) {
    struct foo_object *self = foo_data(h);
    int err;
    if(!self->done) {
        int rc = hdone(h);
        if(rc < 0) {err = errno; goto error;}
    }
    while(1) {
        /* Change to brecvl for bytestream protocols. */
        ssize_t sz = mrecvl(h, NULL, NULL, deadline);
        if(sz < 0 && errno == EPIPE) break;
        if(sz < 0) {err = errno; goto error;}
    }
    hclose(h);
    return 0;
error:
    hclose(h);
    errno = err;
    return -1;
}
```

Overlay protocols implement detach function with very similar semantics:

```
int foo_detach(struct foo_socket *self, int64_t deadline) {
    struct foo_object *self = foo_data(h);
    int err;
    if(!self->done) {
        int rc = hdone(h);
        if(rc < 0) {err = errno; goto error;}
    }
    while(1) {
        /* Change to brecvl for bytestream protocols. */
        ssize_t sz = mrecvl(h, NULL, NULL, deadline);
        if(sz < 0 && errno == EPIPE) break;
        if(sz < 0) {err = errno; goto error;}
    }
    int u = self->underlying;
    free(self);
    return u;
error:
    hclose(h);
    errno = err;
    return -1;
}
```

Successful execution of orderly termination does guarantee that all bytes/messages were read by the peer. However, it does not guarantee that all bytes/messages were actually processed by the peer. This problem is particularly visible when server is shutting down, but it is not addressed in this document. Individual protocols may address the problem by various means, for example by explicitly acknowledging each message, by returning error from close/detach function if there were any unprocessed messages or by returning the sequence number of last processed message.

## Normal operation

Everything that happens between protocol initialization and protocol termination will be referred to as "normal operation".

This section applies only to transport protocols. If functions defined in this section are invoked for an application protocol they MUST fail with ENOTSUP error.

### I/O lists

The functions defined in this section are using iolist structure with the following definition:

```
struct iolist {
    void *iol_base;
    size_t iol_len;
    struct iolist *iol_next;
    int iol_rsvd;
};
```

It is used for the same purpose that structure iovec is used for in classic BSD socket API. However, instead of being assembled in gather/scatter arrays, iolist structures are chained to form singly-linked lists.

* Iol_base points to a buffer
* Iol_len is the size of the buffer pointed to by iol_base
* Iol_next is the next element in the linked list, last element in the list MUST have this fields set to NULL
* Iol_rsvd is reserved and MUST be always set to zero by the caller

Gather/scatter lists are not thread-safe. Functions accepting them as input are allowed to modify them but they MUST restore the list into its original state before returning to the caller. The list MUST be restored to its original state even if the function fails.

Function accepting gather/scatter list as a parameter (unless it just forwards the list to a different function) MUST check whether there is a loop in the list and whether iol_rsvd field for each item of the list is set to zero. If either of those checks fails the function itself MUST fail with EINVAL error.

### Bytestream protocols

Bytestream protocols can be used via following two functions:

```
int bsendl(
    int s,
    struct iolist *first,
    struct iolist *last,
    int64_t deadline);


int brecvl(
    int s,
    struct iolist *first,
    struct iolist *last,
    int64_t deadline);
```

Function bsendl() sends data to the protocol. The protocol SHOULD send them, after whatever manipulation is required, to its underlying protocol. Eventually, the bottommost protocol in the stack sends the data to the network.

Function brecvl() reads data from the protocol. The protocol SHOULD read them from the underlying socket and after whatever required manipulation is done return them to the caller. The bottommost protocol in the stack reads the data from the network.

Both functions above MUST be blocking and exhibit atomic behaviour. I.e. either all data are sent/received or none of them are. In the later case protocol MUST be marked as broken, errno MUST be set to appropriate value and -1 MUST be returned to the user. Any subsequent attempt to use the protocol MUST result in ECONNRESET error.

Expired deadline is considered to be an error and the protocol MUST behave as described above setting errno to ETIMEDOUT.

In case of success both functions MUST return zero.

For both functions, following conditions MUST trigger EINVAL error:

* exactly one of first or last is NULL
* last->iol_next is not NULL
* first and last don't belong to the same list
* iol_rsvd of any element of the list is not zero
* there is a loop in the list

Additionally, bsendl function MUST return error if iol_base field of any of the iolist elements is NULL.

To improve performance these checks SHOULD be delegated to the underlying protocol whenever technically possible.

If iol_base in any element of the list is set to NULL, brecvl must read and drop iol_len bytes from the peer before proceeding to the next element.

If both first and last arguments of brecvl function are set to NULL protocol implementation MUST receive and drop infinite number of bytes. In other words, the function will exit only when there’s an error, the protocol is terminated by the peer or deadline expires.

Note that the implementation of brecvl MAY change the content of the buffer supplied to the function. This is true even in the case of error. However, what exactly will be written into the buffer in case of error is unpredictable and using such data will result in undefined behaviour.

Finally, there are two helper functions for simple cases where the iolist mechanism is not required:

```
int bsend(
    int s, 
    const void *buf,
    size_t len,
    int64_t deadline);


int brecv(
    int s,
    void *buf,
    size_t len,
    int64_t deadline);
```

Semantics of these functions are fully defined by the following pseudocode:

```
int bsend(int h, const void *buf, size_t len, int64_t deadline) {
    struct iolist iol = {(void*)buf, len, NULL, 0};
    return bsendl(h, &iol, &iol, deadline);
}


int brecv(int h, void *buf, size_t len, int64_t deadline) {
    struct iolist iol = {buf, len, NULL, 0};
    return brecvl(h, &iol, &iol, deadline);
}
```

### Message protocols

Message protocols can be used via the following functions:

```
int msendl(int s,
    struct iolist *first,
    struct iolist *last,
    int64_t deadline);
ssize_t mrecvl(
    int s,
    struct iolist *first,
    struct iolist *last,
    int64_t deadline);
```

Function msend() sends message to the protocol. The protocol SHOULD send it, after whatever manipulation is required, to its underlying protocol. Eventually, the lowermost protocol in the stack sends the data to the network.

Function mrecv() reads message from the protocol. The protocol SHOULD read it from its underlying protocol and after whatever manipulation is needed return it to the caller. The lowermost protocol in the stack reads the data from the network.

All the functions MUST be blocking and exhibit atomic behaviour. I.e. either entire message is sent/received or none of it is. In the later case errno MUST be set to appropriate value and -1 MUST be returned to the user. The protocol may be recoverable in which case receiving next message after an error is possible. In can also be non-recoverable in which the protocol MUST be marked as broken and any subsequent attempt to use it MUST result in an ECONNRESET error.

Unlike with bytestream protocols the buffer supplied to mrecv() doesn't have to be fully filled in, i.e. received messages may be smaller than the buffer.

If the message is larger than the buffer, it is considered to be an error and the protocol must return -1 and set errno to EMSGSIZE. If there's no way to discard the unread part of the message in constant time it SHOULD also mark the protocol as broken and refuse any further operations. This behaviour prevents DoS attacks by sending very large messages.

Expired deadline is considered to be an error and the protocol MUST return ETIMEDOUT error.

In case of success msend() function MUST return zero, mrecv() MUST return the size of the received message. Zero is a valid message size.

For both functions, following conditions MUST trigger EINVAL error:

* exactly one of first or last is NULL
* last->iol_next is not NULL
* first and last don't belong to the same list
* iol_rsvd of any element of the list is not zero
* there is a loop in the list

Additionally, msendl function MUST return error if iol_base field of any of the iolist elements is NULL.

To improve performance these checks SHOULD be delegated to the underlying protocol whenever technically possible.

If iol_base in any element of the list is set to NULL, mrecvl must read and drop iol_len bytes of the message before proceeding to the next element.


If both first and last arguments of mrecvl function are set to NULL protocol implementation MUST skip one message.


Note that the implementation of mrecvl function MAY change the content of the buffer supplied to the function. It can do so even in the case of error. However, what exactly will be written into the buffer is unpredictable in case of error and using such data will result in undefined behaviour.

There are also two helper functions for simple cases where the iolist mechanism is not required:

```
int msend(
    int s,
    const void *buf,
    size_t len,
    int64_t deadline);
ssize_t mrecv(
    int s,
    void *buf,
    size_t len,
    int64_t deadline);
```

Semantics of these functions are fully defined by the following pseudocode:

```
int msend(int h, const void *buf, size_t len, int64_t deadline) {
    struct iolist iol = {(void*)buf, len, NULL, 0};
    return msendl(h, &iol, &iol, deadline);
}


ssize_t mrecv(int h, void *buf, size_t len, int64_t deadline) {
    struct iolist iol = {buf, len, NULL, 0};
    return mrecvl(h, &iol, &iol, deadline);
}
```

### Custom sending and receiving functions

In addition to send/recv functions described above, protocols MAY implement their own custom send/recv functions. These functions should be called "send" and/or "recv" (e.g. "udp_send").

Custom functions allow for providing additional arguments. For example, UDP protocol may implement custom send function with additional "destination IP address" argument.

A protocol MAY implement multiple send or receive functions as needed.

Protocol implementors should try to make custom send/recv functions as consistent with standard send/recv as possible. Specifically, there SHOULD be iolist accepting variants of the custom functions.

Standard send/recv functions SHOULD fill in arguments otherwise provided in custom send/recv by sensible defaults. It SHOULD be possible to set those defaults via "start" function.

## Error codes

Functions defined in this specification can return following errors:

* EBADF: Bad file descriptor.
* ECONNRESET: Connection broken. For example, a failure to receive a keepalive from the peer may result in this error. This error is also returned after non-recoverable protocol fails.
* EMSGSIZE: Message is too large. For receive functions this means that the message won’t fit into the supplied buffer. For send functions it means that protocol is not able to transfer message of that size.
* ENOTSUP: The socket does not support the function. For example, msend() was called on a bytestream socket or mrecv() was called on send-only socket.
* EPIPE: The peer have performed orderly shutdown of the connection.
* EPROTO: The peer has violated the protocol specification.
* ETIMEDOUT: Deadline expired.

## IANA Considerations

This memo includes no request to IANA.

## Security Considerations

Network APIs can facilitate DoS attacks by allowing for unlimited buffer sizes and for infinite deadlines.

This proposal avoids the first issue by requiring the user to allocate all the buffers. It addresses the second problem by always making the deadline explicit. Also, by not requiring recomputation of timeout intervals it makes the deadlines easy to use and hard to get wrong. The user should take advantage of that and set reasonable timeout for every network operation.

Other than that, the security implications of the new API don't differ from security implications of classic BSD socket API. Still, it may be worth passing the design through a security audit.

