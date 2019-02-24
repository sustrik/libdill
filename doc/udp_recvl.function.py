
udp_recvl_function = {
    "name": "udp_recvl",
    "topic": "udp",
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
    ],

    "prologue": "This function receives a single UDP packet.",

    "has_handle_argument": True,
    "has_deadline": True,
    "has_iol": True,

    "errors": ["EINVAL", "EMSGSIZE"],
}

new_function(udp_recvl_function)

