
fxs.append(
    {
        "name": "udp_recv",
        "info": "receives an UDP packet",
        "result": {
            "type": "ssize_t",
            "success": "size of the received message, in bytes",
            "error": "-1",
        },
        "args": [
           {
               "name": "s",
               "type": "int",
               "info": "Handle of the UDP socket.",
           },
           {
               "name": "addr",
               "type": "struct ipaddr*",
               "dill": True,
               "info": "Out parameter. IP address of the sender of the packet. " +
                     "Can be **NULL**.",
           },
           {
               "name": "buf",
               "type": "void*",
               "info": "Buffer to receive the data to.",
           },
           {
               "name": "len",
               "type": "size_t",
               "info": "Size of the buffer, in bytes.",
           },
        ],
        "protocol": udp_protocol,
        "prologue": "This function receives a single UDP packet.",

        "has_handle_argument": True,
        "has_deadline": True,

        "errors": ["EINVAL", "EBUSY", "EMSGSIZE"],
    }
)
