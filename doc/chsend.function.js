
newFunction(
    {
        name: "chsend",
        section: "Channels",
        info: "sends a message to a channel",

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
                type: "const void*",
                info: "Pointer to the value to send to the channel."
            },
            {
                name: "len",
                type: "size_t",
                info: "Size of the value to send to the channel, in bytes."
            },
        ],

        has_deadline: true,

        prologue: `
            Sends a message to a channel.

            The size of the message sent to the channel must match the size of
            the message requested from the channel. Otherwise, both peers fail
            with **EMSGSIZE** error. 

            If there's no receiver for the message, the function blocks until
            one shows up or until the deadline expires.
        `,

        has_handle_argument: true,

        errors: ['EINVAL'],

        custom_errors: {
            EMSGSIZE: "The peer expected a different message size.",
            EPIPE: "Channel has been closed with chdone.",
        },

        example: `
            int val = 42;
            int rc = chsend(ch, &val, sizeof(val), now() + 1000);
            if(rc != 0) {
                perror("Cannot send message");
                exit(1);
            }
            printf("Value %d sent successfully.\\n", val);
        `,
    }
)
