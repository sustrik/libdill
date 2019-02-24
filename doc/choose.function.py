
choose_function = {
    "name": "choose",
    "topic": "channel",
    "info": "performs one of multiple channel operations",

    "add_to_synopsis": """
        struct chclause {
            int op;
            int ch;
            void *val;
            size_t len;
        };
    """,

    "result": {
        "type": "int",
        "success": "index of the clause that caused the function to exit",
        "error": "-1",
        "info": """
            Even if an index is returned, **errno** may still be set to
            an error value. The operation was successfull only if **errno**
            is set to zero.
        """,
    },

    "args": [
        {
            "name": "clauses",
            "type": "struct chclause*",
            "dill": True,
            "info": "Operations to choose from. See below."
        },
        {
            "name": "nclauses",
            "type": "int",
            "info": "Number of clauses."
        },
    ],

    "has_deadline": True,

    "prologue": """
        Accepts a list of channel operations. Performs one that can be done
        first. If multiple operations can be done immediately, the one that
        comes earlier in the array is executed.
    """,

    "epilogue": """
        The fields in **chclause** structure are as "follows":

        * **op**: Operation to perform. Either **CHSEND** or **CHRECV**.
        * **ch**: The channel to perform the operation on.
        * **val**: Buffer containing the value to send or receive.
        * **len**: Size of the buffer.
    """,

    "errors": ['EINVAL'],

    "add_to_errors": """
        Additionally, if the function returns an index it can set **errno**
        to one of the following "values":

        * **0**: Operation was completed successfully.
        * **EBADF**: Invalid handle.
        * **EINVAL**: Invalid parameter.
        * **EMSGSIZE**: The peer expected a message with different size.
        * **ENOTSUP**: Operation not supported. Presumably, the handle isn't a channel.
        * **EPIPE**: Channel has been closed with **chdone**.
    """,

    "example": """
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
    """,
}

new_function(choose_function)

