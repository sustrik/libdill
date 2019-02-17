
newFunction(
    {
        name: "chrecv",
        section: "Channels",
        info: "receives a message from a channel",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "ch",
                type: "int",
                info: "The channel."
            },
            {
                name: "val",
                type: "void*",
                info: "The buffer to receive the message to."
            },
            {
                name: "len",
                type: "size_t",
                info: "Size of the buffer, in bytes."
            },
        ],

        has_deadline: true,

        prologue: `
            Receives a message from a channel.

            The size of the message requested from the channel must match the
            size of the message sent to the channel. Otherwise, both peers fail
            with **EMSGSIZE** error. 

            If there's no one sending a message at the moment, the function
            blocks until someone shows up or until the deadline expires.
        `,

        has_handle_argument: true,

        errors: ['EINVAL'],

        custom_errors: {
            EMSGSIZE: "The peer sent a message of different size.",
            EPIPE: "Channel has been closed with chdone.",
        },

        example: `
            int val;
            int rc = chrecv(ch, &val, sizeof(val), now() + 1000);
            if(rc != 0) {
                perror("Cannot receive message");
                exit(1);
            }
            printf("Value %d received.\\n", val);
        `,
    }
)
