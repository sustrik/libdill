
happyeyeballs_connect_function = {
    "name": "happyeyeballs_connect",
    "topic": "tcp",
    "info": "creates a TCP connection using Happy Eyeballs algorithm",

    "result": {
        "type": "int",
        "success": "newly created TCP connection",
        "error": "-1",
    },
    "args": [
        {
            "name": "name",
            "type": "const char*",
            "info": "Name of the host to connect to.",
        },
        {
            "name": "port",
            "type": "int",
            "info": "Port to connect to.",
        }
    ],
    "has_deadline": True,

    "prologue": """
        Happy Eyeballs protocol (RFC 8305) is a way to create a TCP
        connection to a remote endpoint even if some of the IP addresses
        associated with the endpoint are not accessible.

        First, DNS queries to IPv4 and IPv6 addresses are launched in
        parallel. Then a connection is attempted to the first address, with
        IPv6 address being preferred. If the connection attempt doesn't
        succeed in 300ms connection to next address is attempted,
        alternating between IPv4 and IPv6 addresses. However, the first
        connection attempt isn't caneled. If, in the next 300ms, none of the
        previous attempts succeeds connection to the next address is
        attempted, and so on. First successful connection attempt will
        cancel all other connection attemps.

        This function executes the above protocol and returns the newly
        created TCP connection.
    """,

    "errors": ["EINVAL", "EMFILE", "ENFILE", "ENOMEM", "ECANCELED"],

    "example": """
        int s = happyeyeballs_connect("www.example.org", 80, -1);
        int rc = bsend(s, "GET / HTTP/1.1", 14, -1);
    """
}

new_function(happyeyeballs_connect_function)

