tcpmux_protocol = {
    "name": "tcpmux",
    "topic": "TCPMUX protocol",
    "type": "application",
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

protocols.append(tcpmux_protocol)
