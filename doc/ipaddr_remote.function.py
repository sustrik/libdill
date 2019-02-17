
fxs.append(
    {
        "name": "ipaddr_remote",
        "section": "IP addresses",
        "info": "resolve the address of a remote IP endpoint",
        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },
        "args": [
            {
                "name": "addr",
                "type": "struct ipaddr*",
                "info": "Out parameter, The IP address object.",
            },
            {
                "name": "name",
                "type": "const char*",
                "info": "Name of the remote IP endpoint, such as \"www.example.org\" or \"192.168.0.111\".",
            },
            {
                "name": "port",
                "type": "int",
                "info": "Port number. Valid values are 1-65535.",
            },
            {
                "name": "mode",
                "type": "int",
                "info": "What kind of address to return. See above.",
            },
        ],

        "has_deadline": True,

        "prologue": trimrect("""
            Converts an IP address in human-readable format, or a name of a
            remote host into an **ipaddr** structure.
        """) + "\n\n" + trimrect(ipaddr_mode),

        "custom_errors": {
            "EADDRNOTAVAIL": "The name of the remote host cannot be resolved to an address of the specified type.",
        },

        "example": ipaddr_example,
    }
)
