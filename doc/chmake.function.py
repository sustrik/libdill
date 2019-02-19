
fxs.append(
    {
        "name": "chmake",
        "topic": "Channels",
        "info": "creates a channel",

        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },

        "args": [
            {
                "name": "chv",
                "type": "int",
                "suffix": "[2]",
                "info": "Out parameter. Two handles to the opposite ends of the channel."
            },
            {
                "name": "opts",
                "type": "const struct chopts*",
                "dill": True,
                "info": "Options."
            },
        ],

        "prologue": """
            Creates a bidirectional channel. In case of success handles to the
            both sides of the channel will be returned in **chv** parameter.

            A channel is a synchronization primitive, not a container.
            It doesn't store any items.
        """,

        "allocates_handle": True,

        "errors": ['ENOMEM'],

        "example": """
            int ch[2];
            int rc = chmake(ch);
            if(rc == -1) {
                perror("Cannot create channel");
                exit(1);
            }
        """,

        "mem": "chstorage",
    }
)
