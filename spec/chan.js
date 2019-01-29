fxs = [
    {
        name: "chdone",
        section: "Channels",
        info: "half-closes a channel",

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
        ],

        prologue: `
            Closes an outbound half of the channel. When the peer receives
            all the messages sent prior to the calling **chdone** all
            its subsequent attempts to receive will fail with **EPIPE** error.
        `,

        has_handle_argument: true,
        custom_errors: {
            EPIPE: "chdone was already called on this channel.",
        },

        example: `
            int ch[2];
            chmake(ch);
            chsend(ch, "ABC", 3, -1);
            chdone(ch);
        `,
    },
    {
        name: "chmake",
        section: "Channels",
        info: "creates a channel",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },

        args: [
            {
                name: "chv",
                type: "int",
                suffix: "[2]",
                info: "Out parameter. Two handles to the opposite ends of the channel."
            },
        ],

        prologue: `
            Creates a bidirectional channel. In case of success handles to the
            both sides of the channel will be returned in **chv** parameter.

            A channel is a synchronization primitive, not a container.
            It doesn't store any items.
        `,

        allocates_handle: true,

        errors: ['ENOMEM'],

        example: `
            int ch[2];
            int rc = chmake(ch);
            if(rc == -1) {
                perror("Cannot create channel");
                exit(1);
            }
        `,

        mem: "chstorage",
    },
    {
        name: "choose",
        section: "Channels",
        info: "performs one of multiple channel operations",

        add_to_synopsis: `
            struct chclause {
                int op;
                int ch;
                void *val;
                size_t len;
            };
        `,

        result: {
            type: "int",
            success: "index of the clause that caused the function to exit",
            error: "-1",
            info: `
                Even if an index is returned, **errno** may still be set to
                an error value. The operation was successfull only if **errno**
                is set to zero.
            `,
        },

        args: [
            {
                name: "clauses",
                type: "struct chclause*",
                info: "Operations to choose from. See below."
            },
            {
                name: "nclauses",
                type: "int",
                info: "Number of clauses."
            },
        ],

        has_deadline: true,

        prologue: `
            Accepts a list of channel operations. Performs one that can be done
            first. If multiple operations can be done immediately, the one that
            comes earlier in the array is executed.
        `,

        epilogue: `
            The fields in **chclause** structure are as follows:

            * **op**: Operation to perform. Either **CHSEND** or **CHRECV**.
            * **ch**: The channel to perform the operation on.
            * **val**: Buffer containing the value to send or receive.
            * **len**: Size of the buffer.
        `,

        errors: ['EINVAL'],

        add_to_errors: `
            Additionally, if the function returns an index it can set **errno**
            to one of the following values:

            * **0**: Operation was completed successfully.
            * **EBADF**: Invalid handle.
            * **EINVAL**: Invalid parameter.
            * **EMSGSIZE**: The peer expected a message with different size.
            * **ENOTSUP**: Operation not supported. Presumably, the handle isn't a channel.
            * **EPIPE**: Channel has been closed with **chdone**.
        `,

        example: `
            int val1 = 0;
            int val2;
            struct chclause clauses[] = {
                {CHSEND, ch, &val1, sizeof(val1)},
                {CHRECV, ch, &val2, sizeof(val2)}
            };
            int rc = choose(clauses, 2, now() + 1000);
            if(rc == -1) {
                perror("Choose failed");
                exit(1);
            }
            if(rc == 0) {
                printf("Value %d sent.\\n", val1);
            }
            if(rc == 1) {
                printf("Value %d received.\\n", val2);
            }
        `,
    },
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
    },
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
    },

]
