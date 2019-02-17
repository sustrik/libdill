
newFunction(
    {
        name: "udp_recvl",
        info: "receives an UDP packet",
        result: {
            type: "ssize_t",
            success: "size of the received message, in bytes",
            error: "-1",
        },
        args: [
           {
               name: "s",
               type: "int",
               info: "Handle of the UDP socket.",
           },
           {
               name: "addr",
               type: "struct ipaddr*",
               info: "Out parameter. IP address of the sender of the packet. " +
                     "Can be **NULL**.",
           },
        ],
        protocol: udp_protocol,
        prologue: "This function receives a single UDP packet.",

        has_handle_argument: true,
        has_deadline: true,
        has_iol: true,

        errors: ["EINVAL", "EMSGSIZE"],
    }
)
