fxs = [
    {
        name: "mrecvl",
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
        ],
        prologue: `
            This function receives message from a socket. It is a blocking
            operation that unblocks only after entire message is received.
            There is no such thing as partial receive. Either entire message
            is received or no message at all.
        `,

        epilogue: `
            If both **first** and **last** arguments are set to **NULL**
            the message is received and silently dropped. The function will
            still return the size of the message.
        `,

        has_handle_argument: true,
        has_deadline: true,
        has_iol: true,
        uses_connection: true,

        errors: ["EINVAL", "EBUSY", "EMSGSIZE"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },
    },
    {
        name: "msend",
        section: "Message sockets",
        info: "sends a message",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket to send the message to.",
           },
           {
               name: "buf",
               type: "const void*",
               info: "Message to send.",
           },
           {
               name: "len",
               type: "size_t",
               info: "Size of the message, in bytes.",
           },
        ],

        prologue: `
            This function sends a message to a socket. It is a blocking
            operation that unblocks only after entire message is sent. 
            There is no such thing as partial send. If a problem, including
            timeout, occurs while sending the message error is returned to the
            user and the socket cannot be used for sending from that point on.
        `,

        has_handle_argument: true,
        has_deadline: true,
        uses_connection: true,

        errors: ["EINVAL", "EBUSY"],
        custom_errors: {
            EMSGSIZE: "The message is too long.",
            EPIPE: "Closed connection.",
        },

        example: `
            int rc = msend(s, "ABC", 3, -1);
        `,
    },
    {
        name: "msendl",
        section: "Message sockets",
        info: "sends a message",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "The socket to send the message to.",
           },
        ],

        prologue: `
            This function sends a message to a socket. It is a blocking
            operation that unblocks only after entire message is sent. 
            There is no such thing as partial send. If a problem, including
            timeout, occurs while sending the message error is returned to the
            user and the socket cannot be used for sending from that point on.
        `,

        has_handle_argument: true,
        has_deadline: true,
        has_iol: true,
        uses_connection: true,

        errors: ["EINVAL", "EBUSY"],
        custom_errors: {
            EMSGSIZE: "The message is too long.",
            EPIPE: "Closed connection.",
        }
    },
]
