
fxs.append(
    {
        "name": "ipaddr_local",
        "topic": "IP addresses",
        "info": "resolve the address of a local network interface",
        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },
        "args": [
            {
                "name": "addr",
                "type": "struct ipaddr*",
                "dill": True,
                "info": "Out parameter, The IP address object.",
            },
            {
                "name": "name",
                "type": "const char*",
                "info": "Name of the local network interface, such as \"eth0\", \"192.168.0.111\" or \"::1\".",
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

        "prologue": trimrect("""
            Converts an IP address in human-readable format, or a name of a
            local network interface into an **ipaddr** structure.
        """) + "\n\n" + trimrect(ipaddr_mode),

        "custom_errors": {
            "ENODEV": "Local network interface with the specified name does not exist."
        },

        "example": """
            struct ipaddr addr;
            ipaddr_local(&addr, "eth0", 5555, 0);
            int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
            bind(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
        """,
    }
)
