
tcpmux_topic = {
    "name": "tcpmux",
    "title": "TCPMUX protocol",
    "protocol": "application",
    "info": """
       TCPMUX protocol enables multiplexing TCP connections on a single port.
       The connections made on that port are distributed to different processes
       depending on the user-supplied service name.
       The protocol is defined in RFC 1078.
    """,
    "example": """
        TODO
    """,
    "opaque": {"tcpmux_storage" : 1000},
    "opts": {
        "tcpmux_": [
            {
                "name": "mem",
                "type": "struct tcpmux_storage*",
                "dill": True,
                "default": "NULL",
                "info": "Memory to store the object in. If NULL, the memory will be allocated automatically.",
            },
            {
                "name": "addr",
                "type": "const char*",
                "default": '"/tmp/tcpmux"',
                "info": "TODO",
            },
            {
                "name": "port",
                "type": "int",
                "default": "10001",
                "info": "TODO",
            },
        ],
    },
}

new_topic(tcpmux_topic)

