
newFunction(
    {
        name: "brecvl",
        section: "Bytestream sockets",
        info: "receives data from a bytestream socket",
        result: {
            type: "int",
            success: "0",
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
            This function receives data from a bytestream socket. It is a
            blocking operation that unblocks only after the requested amount of
            data is received.  There is no such thing as partial receive.
            If a problem, including timeout, occurs while receiving the data,
            an error is returned to the user and the socket cannot be used for
            receiving from that point on.
        `,

        has_handle_argument: true,
        has_deadline: true,
        has_iol: true,
        uses_connection: true,

        errors: ["EINVAL", "EBUSY"],
        custom_errors: {
            EPIPE: "Closed connection.",
        },
    }
)
