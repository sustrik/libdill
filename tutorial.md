
# Tutorial

## Introduction

In this tutorial, you will develop a simple TCP "greet" server. Clients are meant to connect to it by telnet. After a client has connected, the server will ask for their name, reply with a greeting, and then proceed to close the connection.

An interaction with our server will look like this:

```
$ telnet 127.0.0.1 5555
Trying 127.0.0.1...
Connected to 127.0.0.1.
Escape character is '^]'.
What's your name?
Bartholomaeus
Hello, Bartholomaeus!
Connection closed by foreign host.
```
Throughout the tutorial, you will learn how to use coroutines, channels, and sockets.

## Step 1: Setting up the stage

Start by including the libdill and dsock header files. Later we'll need some functionality from the standard library, so include those headers as well:
    
```c
#include <libdill.h>
#include <dsock.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
```

Add a `main` function. We'll assume that the first argument, if present, will be the port number to be used by the server. If not specified, the port number will default to *5555*:

```c
int main(int argc, char *argv[]) {
    int port = 5555;
    if(argc > 1) port = atoi(argv[1]);

    return 0;
}
```

Now we can move on to the actual interesting stuff.

The `tcp_listen()` function creates a listening TCP socket. The socket can be used to accept new TCP connections from clients:

```c
struct ipaddr addr;
int rc = ipaddr_local(&addr, NULL, port, 0);
if (rc < 0) {
    perror("Can't open listening socket");
    return 1;
}
int ls = tcp_listen(&addr, 10);
assert( ls >= 0 );
```

The `ipaddr_local()` function converts the textual representation of a local IP address to the actual address. Its second argument can be used to specify a local network interface to bind to. This is an advanced feature and you likely won't need it. For now, you can simply ignore it by setting it to `NULL`. This will cause the server to bind to all local network interfaces available.

The third argument is, unsurprisingly, the port that clients will connect to.  When testing the program, keep in mind that valid port numbers range from *1* to *65535* and that binding to ports *1* through *1023* will typically require superuser privileges.

If `tcp_listen()` fails, it will return `-1` and `set errno` to the appropriate error code. The libdill/dscok API is in this respect very similar to standard POSIX APIs. Consequently, we can use standard POSIX error-handling mechanisms such as `perror()` in this case.

As for unlikely errors, the tutorial will simply use `assert`s to catch them so as to stay succinct and readable.

If you run the program at this stage, you'll find out that it terminates immediately without pausing or waiting for a client to connect. That is what the `tcp_accept()` function is for:

```c
int s = tcp_accept(ls, NULL, -1);
assert(s >= 0);
```

The function returns the newly established connection.

Its third argument is a deadline. We'll cover deadlines later on in this tutorial. For now, remember that the constant `-1` can be used to mean 'no deadline' — if there is no incoming connection, the call will block forever.

Finally, we want to handle multiple client connections instead of just one so we put the `tcp_accept()` call into an infinite loop.  For now we'll just print a message when a new connection is established. We will close it immediately and not even check for errors:

```c
while(1) {
    int s = tcp_accept(ls, NULL, -1);
    assert(s >= 0);
    printf("New connection!\n");
    rc = hclose(s);
    assert(rc == 0);
}
```

The source code for this step can be found in the dsock repository under tutorial/step1.c. All the other steps that follow are in the same directory.

Build the program like this:

```
$ gcc -o greetserver step1.c -ldill -ldsock
```

Then run the resulting executable:

```
$ ./greetserver
```

The server is now waiting for a new connection. Use telnet at a different terminal to establish one and then check whether the program works as expected:

```
$ telnet 127.0.0.1 5555
```

To test whether error handling works, try using an invalid port number:

```
$ ./greetserver 70000
Can't open listening socket: Invalid argument
$
```

Everything seems to work as expected. Let's now move on to step 2.

## Step 2: The business logic

When a new connection arrives, the first thing that we want to do is establish the network protocol we'll be using. dsock is a library of easily composable microprotocols that allows you to compose a wide range of protocols just by plugging different microprotocols onto each other in a lego brick fashion.  In this tutorial, however, we are going to limit ourselves to just a very simple setup.  On top of the TCP connection that we've just created, we'll have a simple protocol that will split the TCP bytestream into discrete messages, using line breaks (`CR+LF`) as delimiters:

```c
int s = crlf_start(s);
assert(s >= 0);
```

Note that `hclose()` works recursively.  Given that our CRLF socket wraps an underlying TCP socket, a single call to `hclose()` will close both of them.

Once finished with the setup, we can send the "What's your name?" question to the client:

```c
rc = msend(s, "What's your name?", 17, -1);
if(rc != 0) goto cleanup;
```

Note that `msend()` works with messages, not bytes (the name stands for "message send"). Consequently, the data will act as a single unit: either all of it is received or none of it is. Also, we don't have to care about message delimiters. That's done for us by the CRLF protocol.

To handle possible errors from `msend()` (such as when the client has closed the connection), add a `cleanup` label before the `hclose` line and jump to it whenever you want to close the connection and proceed without crashing the server.

```c
char inbuf[256];
ssize_t sz = mrecv(s, inbuf, sizeof(inbuf), -1);
if(sz < 0) goto cleanup;
```

This piece of code simply reads the reply from the client. The reply is a single message, which in the case of the CRLF protocol translates to a single line of text. The function returns the number of bytes in the message.

Having received a reply, we can now construct the greeting and send it to the client. The analysis of this code is left as an exercise to the reader:

```c
inbuf[sz] = 0;
char outbuf[256];
rc = snprintf(outbuf, sizeof(outbuf), "Hello, %s!", inbuf);
rc = msend(s, outbuf, rc, -1);
if(rc != 0) goto cleanup;
```

Compile the program and check whether it works as expected.

## Step 3: Making it parallel

At this point, the client cannot crash the server, but it can block it. Do the following experiment:

1. Start the server.
2. At a different terminal, start a telnet session and, without entering your name, let it hang.
3. At yet another terminal, open a new telnet session.
4. Observe that the second session hangs without even asking you for your name.

The reason for this behavior is that the program doesn't even start accepting new connections until the entire dialog with the client has finished. What we want instead is to run any number of dialogs with clients in parallel. And that is where coroutines kick in.

Coroutines are defined using the `coroutine` keyword and launched with the `go()` construct.

In our case, we can move the code performing the dialog with the client into a separate function and launch it as a coroutine:

```c
coroutine void dialog(int s) {
    int rc = msend(s, "What's your name?", 17, -1);

    ...

cleanup:
    rc = hclose(s);
    assert(rc == 0);
}

int main(int argc, char *argv[]) {

    ...

    while(1) {
        int s = tcp_accept(ls, NULL, -1);
        assert(s >= 0);
        s = crlf_start(s);
        assert(s >= 0);
        int cr = go(dialog(s));
        assert(cr >= 0);
    }
}
```

Let's compile it and try the initial experiment once again. As can be seen, one client now cannot block another one. Excellent. Let's move on.

NOTE: Please note that for the sake of simplicity the program above doesn't track the running coroutines and close them when they are finished. This results in memory leaks. To understand how should this kind of thing be done properly check the article about [structured concurrency](structured-concurrency.html). 

## Step 4: Deadlines

File descriptors can be a scarce resource. If a client connects to the greetserver and lets the dialog hang without entering a name, one file descriptor on the server side is, for all intents and purposes, wasted.

To deal with the problem, we are going to timeout the whole client/server dialog. If it takes more than *10* seconds, the server will kill the connection at once.

One thing to note is that libdill uses deadlines rather than the more conventional timeouts. In other words, you specify the time instant by which you want the operation to finish rather than the maximum time it should take to run it. To construct deadlines easily, libdill provides the `now()` function. The deadline is expressed in milliseconds, which means you can create a deadline *10* seconds in the future as follows:

```c
int64_t deadline = now() + 10000;
```

Furthermore, you have to modify all potentially blocking function calls in the program to take the deadline parameter. In our case:

```c
int rc = msend(s, "What's your name?", 17, deadline);
if(rc != 0) goto cleanup;
char inbuf[256];
ssize_t sz = mrecv(s, inbuf, sizeof(inbuf), deadline);
if(sz < 0) goto cleanup;
```

Note that `errno` is set to `ETIMEDOUT` if the deadline is reached. Since we're treating all errors the same (by closing the connection), we don't make any specific provisions for deadline expiries.

## Step 5: Communication among coroutines

Suppose we want the greetserver to keep statistics: The overall number of connections made, the number of those that are active at the moment and the number of those that have failed.

In a classic, thread-based application, we would keep the statistics in global variables and synchronize access to them using mutexes.

With libdill, however, we aim at "concurrency by message passing", and so we are going to implement the feature in a different way.

We will create a new coroutine that will keep track of the statistics and a channel that will be used by the `dialog()` coroutines to communicate with it:

<img src="tutorial1.png"/>

First, we define the values that will be passed through the channel:

```c
#define CONN_ESTABLISHED 1
#define CONN_SUCCEEDED 2
#define CONN_FAILED 3
```

Now we can create the channel and pass it to the `dialog()` coroutines:


```c
coroutine void dialog(int s, int ch) {
    ...
}

int main(int argc, char *argv[]) {

    ...

    int ch = chmake(sizeof(int));
    assert(ch >= 0);

    while(1) {
        int s = tcp_accept(ls, NULL, -1);
        assert(s >= 0);
        s = crlf_start(s);
        assert(s >= 0);
        int cr = go(dialog(s, ch));
        assert(cr >= 0);
    }
}
```

The argument to `chmake()` is the type of values that will be passed through the channel. In our case, the type is simply an integer.

Libdill channels are "unbuffered". In other words, the sending coroutine will block each time until the receiving coroutine can process the message.

This kind of behavior could, in theory, become a bottleneck, however, in our case we assume that the `statistics()` coroutine will be extremely fast and not turn into one.

At this point we can implement the `statistics()` coroutine, which will run forever in a busy loop and collect statistics from all the `dialog()` coroutines.  Each time the statistics change, it will print them to `stdout`:

```c
coroutine void statistics(int ch) {
    int active = 0;
    int succeeded = 0;
    int failed = 0;
    
    while(1) {
        int op;
        int rc = chrecv(ch, &op, sizeof(op), -1);
        assert(rc == 0);

        switch(op) {
        case CONN_ESTABLISHED:
            ++active;
            break;
        case CONN_SUCCEEDED:
            --active;
            ++succeeded;
            break;
        case CONN_FAILED:
            --active;
            ++failed;
            break;
        }

        printf("active: %-5d  succeeded: %-5d  failed: %-5d\n",
            active, succeeded, failed);
    }
}

int main(int argc, char *argv[]) {

    ...

    int ch = chmake(sizeof(int));
    assert(ch >= 0);
    int cr = go(statistics(ch));
    assert(cr >= 0);

    ...

}
```

The `chrecv()` function will retrieve one message from the channel or block if there is none available. At the moment we are not sending anything to it, so the coroutine will simply block forever.

To fix that, let's modify the `dialog()` coroutine to send some messages to the channel. The `chsend()` function will be used to do that:

```c
coroutine void dialog(int s, int ch) {
    int op = CONN_ESTABLISHED;
    int rc = chsend(ch, &op, sizeof(op), -1);
    assert(rc == 0);

    ...

cleanup:
    op = errno == 0 ? CONN_SUCCEEDED : CONN_FAILED;
    rc = chsend(ch, &op, sizeof(op), -1);
    assert(rc == 0);
    rc = hclose(s);
    assert(rc == 0);
}
```

Now recompile the server and run it. Create a telnet session and let it time out. The output on the server side will look like this:

```
$ ./greetserver
active: 1      succeeded: 0      failed: 0
active: 0      succeeded: 0      failed: 1
```

The first line is displayed when the connection is established: There is one active connection and no connection has succeeded or failed yet.

The second line shows up when the connection times out: There are no active connection anymore and one connection has failed so far.

Now try pressing enter in telnet when asked for your name. The connection will be terminated by the server immediately, without sending the greeting, and the server log will report one failed connection. What's going on here?

The reason for the behavior is that the CRLF protocol treats an empty line as a connection termination request. Thus, when you press enter in telnet, you send an empty line which causes `mrecv()` on the server side to return the `EPIPE` error, which represents "connection terminated by peer". The server will jump directly into to the cleanup code.

And that's the end of the tutorial. Enjoy your time with the library!
