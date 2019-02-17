
newFunction(
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
    }
)
