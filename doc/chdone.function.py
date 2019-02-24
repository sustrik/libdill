
chdone_function = {
    "name": "chdone",
    "topic": "channel",
    "info": "half-closes a channel",

    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },

    "args": [
        {
            "name": "ch",
            "type": "int",
            "info": "The channel."
        },
    ],

    "prologue": """
        Closes an outbound half of the channel. When the peer receives
        all the messages sent prior to the calling **chdone** all
        its subsequent attempts to receive will fail with **EPIPE** error.
    """,

    "has_handle_argument": True,
    "custom_errors": {
        "EPIPE": "chdone was already called on this channel.",
    },

    "example": """
        int ch[2];
        chmake(ch);
        chsend(ch, "ABC", 3, -1);
        chdone(ch);
    """,
}

new_function(chdone_function)

