
fxs.append(
    {
        "name": "hclose",
        "topic": "Handles",
        "info": "hard-closes a handle",

        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },

        "args": [
            {
                "name": "h",
                "type": "int",
                "info": "The handle.",
            },
        ],
    
        "has_handle_argument": True,

        "prologue": """
            This function closes a handle. Once all handles pointing to the same
            underlying object have been closed, the object is deallocated
            immediately, without blocking.

            The  function  guarantees that all associated resources are
            deallocated. However, it does not guarantee that the handle's work
            will have been fully finished. E.g., outbound network data may not
            be flushed.

            In the case of network protocol sockets the entire protocol stack
            is closed, the topmost protocol as well as all the protocols
            beneath it.
        """,

        "example": """
            int ch[2];
            chmake(ch);
            hclose(ch[0]);
            hclose(ch[1]);
        """
    }
)
