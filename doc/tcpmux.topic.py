
tcpmux_topic = {
    "name": "tcpmux",
    "title": "TCPMUX protocol",
    "opts": ["tcpmux"],
    "storage": {"tcpmux" : 1000},
    "protocol": "application",
    "info": """
       TCPMUX protocol enables multiplexing TCP connections on a single port.
       The connections made on that port are distributed to different processes
       depending on the user-supplied service name.
       The protocol is defined in RFC 1078.
    """,
    "example": """
        TODO
    """
}

new_topic(tcpmux_topic)

