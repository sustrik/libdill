
fxs.append(
    {
        "name": "hown",
        "section": "Handles",
        "info": "transfer ownership of a handle",

        "result": {
            "type": "int",
            "success": "new handle",
            "error": "-1",
        },
        "args": [
            {
                "name": "h",
                "type": "int",
                "info": "Handle to transfer.",
            }
        ],

        "allocates_handle": True,

        "prologue": """
            Handles are integers. You own a handle if you know its numeric
            value. To transfer ownership this function changes the number
            associated with the object. The old number becomes invalid and
            using it will result in undefined behavior. The new number can
            be used in exactly the same way as the old one would.

            If the function fails, the old handle is closed.
        """,
        "errors": ["EBADF"],

        "example": """
            int h1 = tcp_connect(&addr, deadline);
            int h2 = hown(h1);
            /* h1 is invalid here */
            hclose(h2);        """
    }
)
