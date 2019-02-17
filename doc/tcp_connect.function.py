
fxs.append(
    {
        "name": "tcp_connect",
        "info": "creates a connection to remote TCP endpoint ",

        "result": {
            "type": "int",
            "success": "newly created socket handle",
            "error": "-1",
        },
        "args": [
            {
                "name": "addr",
                "type": "const struct ipaddr*",
                "info": "IP address to connect to.",
            },
        ],

        "has_deadline": True,

        "protocol": tcp_protocol,

        "prologue": """
            This function creates a connection to a remote TCP endpoint.
        """,
        "epilogue": """
            The socket can be cleanly shut down using **tcp_close** function.
        """,

        "allocates_handle": True,
        "mem": "tcp_storage",

        "errors": ["EINVAL"],
        "custom_errors": {
            "ECONNREFUSED": "The target address was not listening for connections or refused the connection request.",
            "ECONNRESET": "Remote host reset the connection request.",
            "EHOSTUNREACH": "The destination host cannot be reached.",
            "ENETDOWN": "The local network interface used to reach the destination is down.",
            "ENETUNREACH": "No route to the network is present.",
        },

        "example": """
            struct ipaddr addr;
            ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
            int s = tcp_connect(&addr, -1);
            bsend(s, "ABC", 3, -1);
            char buf[3];
            brecv(s, buf, sizeof(buf), -1);
            tcp_close(s);
        """
    }
)
