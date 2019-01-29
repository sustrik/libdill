fxs = [
    {
        name: "ipc_accept",
        info: "accepts an incoming IPC connection",

        result: {
            type: "int",
            success: "handle of the new connection",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "Socket created by **ipc_listen**.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function accepts an incoming IPC connection.
        `,
        epilogue: `
            The socket can be cleanly shut down using **ipc_close** function.
        `,

        has_handle_argument: true,
        allocates_handle: true,
        mem: "ipc_storage",
    },
    {
        name: "ipc_close",
        info: "closes IPC connection in an orderly manner",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "The IPC socket.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function closes a IPC socket cleanly. Unlike **hclose** it lets
            the peer know that it is shutting down and waits till the peer
            acknowledged the shutdown. If this terminal handshake cannot be
            done it returns error. The socket is closed even in the case of
            error.

            It can also be used to close IPC listener socket in which case it's
            equivalent to calling **hclose**.
        `,

        has_handle_argument: true,
        custom_errors: {
            ECONNRESET: "Broken connection.",
        },
    },
    {
        name: "ipc_connect",
        info: "creates a connection to remote IPC endpoint",

        result: {
            type: "int",
            success: "newly created socket handle",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "const char*",
                info: "Filename to connect to.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function creates a connection to a remote IPC endpoint.
        `,
        epilogue: `
            The socket can be cleanly shut down using **ipc_close** function.
        `,

        allocates_handle: true,
        mem: "ipc_storage",

        errors: ["EINVAL"],
        custom_errors: {
            EACCES: "The process does not have appropriate privileges.",
            ECONNREFUSED: "The target address was not listening for connections or refused the connection request.",
            ECONNRESET: "Remote host reset the connection request.",
            ELOOP: "A loop exists in symbolic links encountered during resolution of the pathname in address.",
            ENAMETOOLONG: "A component of a pathname exceeded **NAME_MAX** characters, or an entire pathname exceeded **PATH_MAX** characters.",
            ENOENT: "A component of the pathname does not name an existing file or the pathname is an empty string.",
            ENOTDIR: "A component of the path prefix of the pathname in address is not a directory.",
        },

        example: `
            int s = ipc_connect("/tmp/test.ipc", -1);
            bsend(s, "ABC", 3, -1);
            char buf[3];
            brecv(s, buf, sizeof(buf), -1);
            ipc_close(s);
        `
    },
    {
        name: "ipc_done",
        info: "half-closes a IPC connection",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "s",
                type: "int",
                info: "The IPC connection handle.",
            },
        ],

        has_deadline: true,

        protocol: ipc_protocol,

        prologue: `
            This function closes the outbound half of ICP connection.
            This will cause the peer to get **EPIPE** error after it has
            received all the data. 
        `,

        has_handle_argument: true,

        custom_errors: {
            EPIPE: "The connection was already half-closed.",
        },
    },
    {
        name: "ipc_fromfd",
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
                info: "File descriptor of a connected UNIX doemain socket to wrap.",
            },
        ],

        protocol: ipc_protocol,

        prologue: `
            This function wraps an existing OS-level file descriptor.
        `,
        epilogue: `
            The socket can be cleanly shut down using **ipc_close** function
            which will also close the underlying file descriptor.

            There's no way to unwrap the file descriptor.
        `,

        allocates_handle: true,
        mem: "ipc_storage",

        example: `
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            connect(fd, addr, sizeof(addr));
            int s = ipc_fromfd(fd);
            bsend(s, "ABC", 3, -1);
            char buf[3];
            brecv(s, buf, sizeof(buf), -1);
            ipc_close(s);
        `
    },
    {
        name: "ipc_listen",
        info: "starts listening for incoming IPC connections",

        result: {
            type: "int",
            success: "newly created socket",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "const char*",
                info: "The filename to listen on.",
            },
            {
                name: "backlog",
                type: "int",
                info: "Maximum number of connections that can be kept open without accepting them.",
            },
        ],

        protocol: ipc_protocol,

        prologue: `
            This function starts listening for incoming IPC connections.
            The connections can be accepted using **ipc_accept** function.
        `,
        epilogue: `
            The socket can be closed either by **hclose** or **ipc_close**.
            Both ways are equivalent.
        `,

        allocates_handle: true,
        mem: "ipc_listener_storage",

        errors: ["EINVAL"],
        custom_errors: {
            EACCES: "The process does not have appropriate privileges.",
            EADDRINUSE: "The specified address is already in use.",
            ELOOP: "A loop exists in symbolic links encountered during resolution of the pathname in address.",
            ENAMETOOLONG: "A component of a pathname exceeded **NAME_MAX** characters, or an entire pathname exceeded **PATH_MAX** characters.",
            ENOENT: "A component of the pathname does not name an existing file or the pathname is an empty string.",
            ENOTDIR: "A component of the path prefix of the pathname in address is not a directory.",
        },
    },
    {
        name: "ipc_listener_fromfd",
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
                info: "File descriptor of a listening UNIX domain socket to wrap.",
            },
        ],

        protocol: ipc_protocol,

        prologue: `
            This function wraps an existing OS-level file descriptor.
        `,
        epilogue: `
            The socket can be cleanly shut down using **ipc_close** function
            which will also close the underlying file descriptor.

            There's no way to unwrap the file descriptor.
        `,

        allocates_handle: true,
        mem: "ipc_listener_storage",

        example: `
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            bind(fd, addr, sizeof(addr));
            listen(fd, 10);
            int s = ipc_listener_fromfd(fd);
        `
    },
    {
        name: "ipc_pair",
        section: "IPC protocol",
        info: "creates a pair of mutually connected IPC sockets",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "s",
                type: "int",
                suffix: "[2]",
                info: "Out parameter. Two handles to the opposite ends of the connection."
            },
        ],

        prologue: `
            This function creates a pair of mutually connected IPC sockets.
        `,
        epilogue: `
            The sockets can be cleanly shut down using **ipc_close** function.
        `,

        allocates_handle: true,

        errors: ['ECANCELED'],

        example: `
              int s[2];
              int rc = ipc_pair(s);
        `,

        mem: "ipc_pair_storage",
    },
    {
        name: "mrecv",
        section: "Message sockets",
        info: "receives a message",
        result: {
            type: "ssize_t",
            success: "size of the received message, in bytes",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket.",
           },
           {
               name: "buf",
               type: "void*",
               info: "Buffer to receive the message to.",
           },
           {
               name: "len",
               type: "size_t",
               info: "Size of the buffer, in bytes.",
           },
        ],
        prologue: `
            This function receives message from a socket. It is a blocking
            operation that unblocks only after entire message is received.
            There is no such thing as partial receive. Either entire message
            is received or no message at all.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        errors: ["EINVAL", "EBUSY", "EMSGSIZE"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },

        example: `
            char msg[100];
            ssize_t msgsz = mrecv(s, msg, sizeof(msg), -1);
        `,
    },
]
