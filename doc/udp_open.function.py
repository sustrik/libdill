
fxs.append(
    {
        "name": "udp_open",
        "info": "opens an UDP socket",
        "result": {
            "type": "int",
            "success": "newly created socket handle",
            "error": "-1",
        },
        "args": [
           {
               "name": "local",
               "type": "struct ipaddr*",
               "dill": True,
               "info": "IP  address to be used to set source IP address in " +
                     "outgoing packets. Also, the socket will receive " +
                     "packets sent to this address. If port in the address " +
                     "is set to zero an ephemeral port will be chosen and " +
                     "filled into the local address.",
           },
           {
               "name": "remote",
               "type": "const struct ipaddr*",
               "dill": True,
               "info": "IP address used as default destination for outbound " +
                     "packets. It is used when destination address in " +
                     "**udp_send** function is set to **NULL**. It is also " +
                     "used by **msend** and **mrecv** functions which don't " +
                     "allow to specify the destination address explicitly. " +
                     "Furthermore, if remote address is set, all the " +
                     "packets arriving from different addresses will be " +
                     "silently dropped.",
           },
           {
               "name": "opts",
               "type": "const struct udp_opts*",
               "dill": True,
               "info": "Options.",
           },
        ],
        "protocol": udp_protocol,
        "prologue": "This function creates an UDP socket.",
        "epilogue": "To close this socket use **hclose** function.",

        "allocates_handle": True,
        "mem": "udp_storage",

        "errors": ["EINVAL"],
        "custom_errors": {
            "EADDRINUSE": "The local address is already in use.",
            "EADDRNOTAVAIL": "The specified address is not available from the " +
                           "local machine.",
        },
    },
)
