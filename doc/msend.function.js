
newFunction(
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
    }
)
