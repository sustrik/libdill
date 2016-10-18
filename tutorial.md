
# Tutorial

## Introduction

In this tutorial you will develop a simple TCP "greet" server. The client is meant to connect to it via telnet. After doing so, the server will ask for their name, reply with a greeting and close the connection.

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

In the process you will learn how to use coroutines, channels and sockets.

## Step 1: Setting up the stage

First, include libdill and dsock header files. Later on we'll need some functionality from the standard library, so include those headers as well:
    
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

Add the `main` function. We'll assume, that the first argument, if present, will be the port number to be used by the server. If not specified, we will default to 5555:

```c
int main(int argc, char *argv[]) {
    int port = 5555;
    if(argc > 1) port = atoi(argv[1]);

    return 0;
}
```

Now we can start doing the actual interesting stuff.

`tcp_listen()` function can be used to create listening TCP socket. The socket will be used to accept new TCP connections from the clients:

```c
ipaddr addr;
int rc = ipaddr_local(&addr, NULL, port, 0);
assert(rc == 0);
int ls = tcp_listen(&addr, 10);
if(ls < 0) {
    perror("Can't open listening socket");
    return 1;
}
```

`ipaddr_local()` function is used to convert textual representation of a local IP address to the actual address. Second argument can be used to specify local network interface to bind to. This is an advanced functionality and you likely won't need it. Conveniently, you can just ignore it and set the argument to `NULL`. The server will then bind to all available local network interfaces.

The third argument is, unsurprisingly, the port that the clients will connect to. When testing the program keep in mind that the range of valid port numbers is 1 to 65535 and binding to the ports from 1 to 1023 typically requires superuser privileges.

If `tcp_listen()` fails it returns -1 and sets `errno` to appropriate error code. Libdill/dscok API is in this respect very similar to standard POSIX APIs. What it means is that we can use standard POSIX mechanims -- `perror()` in this case -- to deal with errors.

As for errors that are not likely, we'll just use asserts to catch them, so that the tutorial code is kept succint and readable.

If you run the program at this stage you'll find out that it finishes immediately rather than pausing and waiting for a connection from the client to arrive. That is what `tcp_accept()` function is for:

```c
int s = tcp_accept(ls, NULL, -1);
assert(s >= 0);
```

The function returns the newly established connection.

The third argument of the function is a deadline. We'll cover the deadlines later on in this tutorial. For now, remember that constant -1 can be used to mean 'no deadline' -- if there is no incoming connection the call will block forever.

Finally, we want to handle many connections from the clients rather than a single one so we put the `tcp_accept()` call into an infinite loop.

For now we'll just print a message when new connection is established we won't even check for error and close it immediately:

```c
while(1) {
    int s = tcp_accept(ls, NULL, -1);
    assert(s >= 0);
    printf("New connection!\n");
    rc = hclose(s);
    assert(rc == 0);
}
```

The source code for this step can be found in dsock repository named `tutorial/step1.c`. All the following steps will be available in the same directory.

Build it like this:

```
$ gcc -o greetserver step1.c -ldill -ldsock
```

Then run the resulting executable:

```
$ ./greetserver
```

The server now waits for a new connection. Establish one from a different terminal using telnet and check whether it works like expected:

```
$ telnet 127.0.0.1 5555
```

To test whether error hadling works all right try to use invalid port number:

```
$ ./greetserver 70000
Can't open listening socket: Invalid argument
$
```

Everyting seems to work as expected. Let's now move to the step 2.

## Step 2: The business logic

When new connection arrives, the first thing that we want to do is to set up the network protocol we'll use. dsock is a library of easily composable microprotocols and thus you can create a wide range of protocols just by plugging different microprotocols each to another, in lego brick fashion. However, in this tutorial we are going to use just a very simple setup. There's the TCP connection we've just created and on top there will be a simple protocol that delimits the TCP bytestream into discrete messages using line breaks (CR+LF) as delimiters:

```c
int s = crlf_start(s);
assert(s >= 0);
```

Note that `hclose()` works in recursive manner. Given that our CRLF socket wraps the underlying TCP socket, single call to `hclose()` will close both of them.

Once done we can send the "What's your name?" question to the client:

```c
rc = msend(s, "What's your name?", 17, -1);
if(rc != 0) goto cleanup;
```

Note that `msend()` works with messages, not bytes (the name stands for "message send"). Therefore, the data acts as a single unit: either all of it is received or none of it. Also, we don't have to care about delimitation of messages. That's done for us by the CRLF protocol.

To handle possible errors from `msend()` (such as when the client closes the connection) add `cleanup` label before the `hclose` line and jump to it whenever you want to close the connection and proceed without crashing the server.

```c
char inbuf[256];
ssize_t sz = mrecv(s, inbuf, sizeof(inbuf), -1);
if(sz < 0) goto cleanup;
```

This piece of code simply reads the reply from the client. Reply is a single message which in case of CRLF protocol translates to a single line of text. The function returns number of bytes in the message.

Having received the reply from the client, we can now construct the greeting and send it to the client. The analysis of this code is left as an exercise to the reader:

```c
inbuf[sz] = 0;
char outbuf[256];
rc = snprintf(outbuf, sizeof(outbuf), "Hello, %s!", inbuf);
rc = msend(s, outbuf, rc, -1);
if(rc != 0) goto cleanup;
```

Compile the program and check whether it works like expected!

## Step 3: Making it parallel

At this point the client can't crash the server, but it can block it. Do the following experiment:

1. Start the server.
2. From a different terminal start a telnet session and let it hang without entering your name.
3. From yet different terminal open a new telnet session.
4. Observe that the second session hangs without even asking you for your name.

The reason for the behaviour is that the program doesn't even start accepting new connection until the entire dialogue with the client is finished. What we want instead is running any number of dialogues with the clients in parallel. And that is where coroutines kick in.

Coroutines are very much like threads. They are very lightweight though. Measuring on a modern hardware you can run up to something like twenty million libdill coroutines per second.

Coroutines are defined using `coroutine` keyword and launched using `go()` construct.

In our case we can move the code performing the dialogue with the client into a separate function and launch it as a coroutine:

```c
coroutine void dialogue(int s) {
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
        int cr = go(dialogue(s));
        assert(cr >= 0);
    }
}
```

Let's compile it and try the initial experiment once again. As can be seen, one client now cannot block another one. Great. Let's move on.

## Step 4: Deadlines

File descriptors can be a scarce resource. If a client connects to greetserver and lets the dialogue hang without entering the name, one file descriptor on the server side is, for all practical purposes, wasted.

To deal with the problem we are going to timeout the whole client/server dialogue. If it takes more than 10 seconds, server will kill the connection straight away.

One thing to note is that libdill uses deadlines rather than more conventional timeouts. In other words, you specify the time instant when you want the operation to finish rather than maximum time it should take to run. To construct deadlines easily, libdill provides `now()` function. The deadline is expressed in milliseconds so you can create a deadline 10 seconds in the future like this:

```c
int64_t deadline = now() + 10000;
```

Further, you have to modify all the potentially blocking function calls in the program to take the deadline parameter. For example:

```c
int rc = msend(s, "What's your name?", 17, deadline);
if(rc != 0) goto cleanup;
```

Note that `errno` is set to `ETIMEDOUT` in case the deadline is reached. However, we treat all the errors in the same way (by closing the connection) and thus we don't have to do any specific provisions for the deadline case.

## Step 5: Communicating among coroutines

Imagine we want to keep statistics in the greetserver: Number of overall connections, number of those that are active at the moment and number of those that have failed.

In a classic thread-based application we would keep the statistics in global variables and synchronise access to them using mutexes.

With libdill, however, we are aiming at "concurrency by message passing" and thus we are going to implement the feature in a different way.

We will create a new coroutine that will keep track of the statistics and a channel that will be used by `dialogue()` coroutines to communicate with it:

<img src="tutorial1.jpeg"/>

First, we define values that will be passed through the channel:

```c
#define CONN_ESTABLISHED 1
#define CONN_SUCCEEDED 2
#define CONN_FAILED 3
```

Now we can create the channel and pass it to `dialogue()` coroutines:


```c
coroutine void dialogue(int s, int ch) {
    ...
}

int main(int argc, char *argv[]) {

    ...

    int ch = channel(sizeof(int), 0);
    assert(ch >= 0);

    while(1) {
        int s = tcp_accept(ls, NULL, -1);
        assert(s >= 0);
        s = crlf_start(s);
        assert(s >= 0);
        int cr = go(dialogue(s, ch));
        assert(cr >= 0);
    }
}
```

The first argument to `channel()` is the type of the values that will be passed through the channel. In our case, they are simple integers.

The second argument is the size of channel's buffer. Setting it to zero means that the channel is "unbuffered" or, in other words, that the sending coroutine will block each time until the receiving coroutine can process the message.

This kind of behaviour could, in theory, become a bottleneck, however, in our case we assume that `statistics()` coroutine will be extremely fast and not likely to turn into a performance bottleneck.

At this point we can implement the `statistics()` coroutine that will run forever in a busy loop and collect the statistics from all the `dialogue()` coroutines. Each time the statistics change, it will print them to `stdout`:

```c
coroutine void statistics(int ch) {
    int connections = 0;
    int active = 0;
    int failed = 0;
    
    while(1) {
        int op;
        int rc = chrecv(ch, &op, sizeof(op), -1);
        assert(rc == 0);

        if(op == CONN_ESTABLISHED)
            ++connections, ++active;
        else
            --active;
        if(op == CONN_FAILED)
            ++failed;

        printf("Total number of connections: %d\n", connections);
        printf("Active connections: %d\n", active);
        printf("Failed connections: %d\n\n", failed);
    }
}

int main(int argc, char *argv[]) {

    ...

    int ch = channel(sizeof(int), 0);
    assert(ch >= 0);
    int cr = go(statistics(ch));
    assert(cr >= 0);

    ...

}
```

`chrecv()` function will retrieve one message from the channel or block if there is none available. At the moment we are not sending anything to the channel, so the coroutine will simply block forever.

To fix that, let's modify `dialogue()` coroutine to send some messages to the channel. `chsend()` function will be used to do that:

```c
coroutine void dialogue(int s, int ch) {
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

Now compile the server and run it. Create a telnet session and let it timeout. The output on the server side will look like this:

```
$ ./greetserver
Total number of connections: 1
Active connections: 1
Failed connections: 0

Total number of connections: 1
Active connections: 0
Failed connections: 1
```

The first block of text is displayed when the connection is established: There have been 1 connection ever and 1 is active at the moment.

Second block shows up when the connection times out: There have been 1 connection overall, there are no active connection any longer and one connection have failed in the past.

Now try just pressing enter when asked for the name. The connection is terminated by server immediately, without sending the greeting and the server log reports one failed connection. What's going on here?

The reason for the bahavior is that CRLF protocol treats an empty line as a connection termination request. Thus, when you press enter in telnet, you send an empty line whicha causes `mrecv()` on the server side to return `EPIPE` error which stands for "connection terminated by the peer". Server thus jumps directly to the cleanup code.


## Step 6: Multi-process server

Our new server does what it is supposed to do. With no context switching it has performance that can never be matched by a multithreaded server. But it has one major drawback. It runs only on one CPU core. Even if the machine has 64 cores it runs on one of them and leaves 63 cores unutilised.

If the server is IO-bound, as it happens to be in this tutorial, it doesn't really matter. However, if it was CPU-bound we would face a serious problem.

What we are going to do in this step is to create one server process per CPU core and distribute the incoming connections among the processes. That way, each process can run on one CPU core, fully utilising resources of the machine.

It is important to note that spawning more processes than there are CPU cores is not desirable. It results in unnecessary context switching among processes located on a single core and ultimately to the performance degradation.

First, let's do some preparatory work. We want statistics to print out the process ID so that we can distinguish between outputs from different instances of the server. (And yes, it would be better to aggregate the statistics from all instances, but that's out of scope of this tutorial and left as an exercise to the reader.)

```c
coroutine void statistics(int ch) {

    ...

    printf("Process ID: %d\n", (int)getpid());
    printf("Total number of connections: %d\n", connections);
    printf("Active connections: %d\n", active);
    printf("Failed connections: %d\n\n", failed);

    ...

}
```

Second, we won't try to estimate the number of CPU cores on the machine and rather let the user specify it:

```c
int main(int argc, char *argv[]) {
    int port = 5555;
    int nproc = 1;
    if(argc > 1)
        port = atoi(argv[1]);
    if(argc > 2)
        nproc = atoi(argv[2]);
    ...
}
```

Third, let's move the connection accepting loop to a separate coroutine:

```c
coroutine void accepter(int ls, int ch) {
    while(1) {
        int s = tcp_accept(ls, NULL, -1);
        assert(s >= 0);
        s = crlf_start(s);
        assert(s >= 0);
        int cr = go(dialogue(s, ch));
        assert(cr >= 0);
    }
}
```

Now we can do the actual magic. The idea is to open the listening socket in the first process, then to fork all the remaining processes, each inheriting the socket. Each process can then `tcp_accept()` connections from the listening socket making OS work as a simple load balancer, dispatching connections evenly among processes. Actually, we will fork number of processes requested by the user short by one. The reason is that the parent process itself is going to act as one instance of the server after if finishes the fork loop.

```c
int main(int argc, char *argv[]) {

    ...

    int i;
    for (i = 0; i < nproc - 1; ++i) {
        cr = proc(accepter(ls, ch));
        assert(cr >= 0);
    }
    accepter(ls, ch);
}
```

An interesting thing to notice in the code above is that coroutines are perfectly normal C functions. While they can be invoked using `go()` and `proc()` constructs they can also be called in standard C way.

Now we can check it in practice. Let's run it on 4 CPU cores:

```
./greetserver 5555 4
```

Check the processes from a different terminal using `ps`:

```
$ ps -a
  PID TTY          TIME CMD
16406 pts/14   00:00:00 greetserver
16420 pts/14   00:00:00 greetserver
16421 pts/14   00:00:00 greetserver
16422 pts/14   00:00:00 greetserver
16424 pts/7    00:00:00 ps
```

Telnet to the server twice to see whether the connections are dispatched to different instances:

```
Process ID: 16420
Total number of connections: 1
Active connections: 1
Failed connections: 0

Process ID: 16422
Total number of connections: 1
Active connections: 1
Failed connections: 0
```

Yay! Everything works as expected.

We are done with the tutorial now. Enjoy your time with the library!

