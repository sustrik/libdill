fxs = [
    {
        name: "tcp_accept",
        info: "accepts an incoming TCP connection",

        result: {
            type: "int",
            success: "handle of the new connection",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "Socket created by **tcp_listen**.",
            },
            {
                name: "addr",
                type: "struct ipaddr*",
                info: "Out parameter. IP address of the connecting endpoint. Can be **NULL**.",
            },
        ],

        has_deadline: true,

        protocol: tcp_protocol,

        prologue: `
            This function accepts an incoming TCP connection.
        `,
        epilogue: `
            The socket can be cleanly shut down using **tcp_close** function.
        `,

        has_handle_argument: true,
        allocates_handle: true,
        mem: "tcp_storage",

        errors: ["ECANCELED"],
    },
    {
        name: "tcp_close",
        info: "closes TCP connection in an orderly manner",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "The TCP socket.",
            },
        ],

        has_deadline: true,

        protocol: tcp_protocol,

        prologue: `
            This function closes a TCP socket cleanly. Unlike **hclose** it lets
            the peer know that it is shutting down and waits till the peer
            acknowledged the shutdown. If this terminal handshake cannot be
            done it returns error. The socket is closed even in the case of
            error.

            It can also be used to close TCP listener socket in which case it's
            equivalent to calling **hclose**.
        `,

        has_handle_argument: true,
        custom_errors: {
            ECONNRESET: "Broken connection.",
        },
    },
    {
        name: "tcp_connect",
        info: "creates a connection to remote TCP endpoint ",

        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address to connect to.",
            },
        ],

        has_deadline: true,

        protocol: tcp_protocol,

        prologue: `
            This function creates a connection to a remote TCP endpoint.
        `,
        epilogue: `
            The socket can be cleanly shut down using **tcp_close** function.
        `,

        allocates_handle: true,
        mem: "tcp_storage",

        errors: ["EINVAL"],
        custom_errors: {
            ECONNREFUSED: "The target address was not listening for connections or refused the connection request.",
            ECONNRESET: "Remote host reset the connection request.",
            EHOSTUNREACH: "The destination host cannot be reached.",
            ENETDOWN: "The local network interface used to reach the destination is down.",
            ENETUNREACH: "No route to the network is present.",
        },

        example: `
            struct ipaddr addr;
            ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
            int s = tcp_connect(&addr, -1);
            bsend(s, "ABC", 3, -1);
            char buf[3];
            brecv(s, buf, sizeof(buf), -1);
            tcp_close(s);
        `
    },
    {
        name: "tcp_done",
        info: "half-closes a TCP connection",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "The TCP connection handle.",
            },
        ],

        has_deadline: true,

        protocol: tcp_protocol,

        prologue: `
            This function closes the outbound half of TCP connection.
            Technically, it sends a FIN packet to the peer. This will, in turn,
            cause the peer to get **EPIPE** error after it has received all
            the data. 
        `,

        has_handle_argument: true,

        custom_errors: {
            EPIPE: "The connection was already half-closed.",
        },
    },
    {
        name: "tcp_fromfd",
        info: "wraps an existing OS-level file descriptor",

        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
            {
                name: "fd",
                type: "int",
                info: "File descriptor of a connected TCP socket to wrap.",
            },
        ],

        protocol: tcp_protocol,

        prologue: `
            This function wraps an existing OS-level file descriptor.
        `,
        epilogue: `
            The socket can be cleanly shut down using **tcp_close** function
            which will also close the underlying file descriptor.

            There's no way to unwrap the file descriptor.
        `,

        allocates_handle: true,
        mem: "tcp_storage",

        example: `
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            connect(fd, addr, sizeof(addr));
            int s = tcp_fromfd(fd);
            bsend(s, "ABC", 3, -1);
            char buf[3];
            brecv(s, buf, sizeof(buf), -1);
            tcp_close(s);
        `
    },
    {
        name: "tcp_listen",
        info: "starts listening for incoming TCP connections",

        result: {
            type: "int",
            success: "newly created socket",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address to listen on.",
            },
            {
                name: "backlog",
                type: "int",
                info: "Maximum number of connections that can be kept open without accepting them.",
            },
        ],

        protocol: tcp_protocol,

        prologue: `
            This function starts listening for incoming connections.
            The connections can be accepted using **tcp_accept** function.
        `,
        epilogue: `
            The socket can be closed either by **hclose** or **tcp_close**.
            Both ways are equivalent.
        `,

        allocates_handle: true,
        mem: "tcp_listener_storage",

        errors: ["EINVAL"],
        custom_errors: {
            EADDRINUSE: "The specified address is already in use.",
            EADDRNOTAVAIL: "The specified address is not available from the local machine.",
        },
    },
    {
        name: "tcp_listener_fromfd",
        info: "wraps an existing OS-level file descriptor",

        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
            {
                name: "fd",
                type: "int",
                info: "File descriptor of a listening TCP socket to wrap.",
            },
        ],

        protocol: tcp_protocol,

        prologue: `
            This function wraps an existing OS-level file descriptor.
        `,
        epilogue: `
            The socket can be cleanly shut down using **tcp_close** function
            which will also close the underlying file descriptor.

            There's no way to unwrap the file descriptor.
        `,

        allocates_handle: true,
        mem: "tcp_listener_storage",

        example: `
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            bind(fd, addr, sizeof(addr));
            listen(fd, 10);
            int s = tcp_listener_fromfd(fd);
        `
    },
]
