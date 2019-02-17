
newFunction(
    {
        name: "ipaddr_sockaddr",
        section: "IP addresses",
        info: "returns sockaddr structure corresponding to the IP address",
        result: {
            type: "const struct sockaddr*",
            info: "Pointer to **sockaddr** structure correspoding to the address object.",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
        ],

        prologue: `
            Returns **sockaddr** structure corresponding to the IP address.
            This function is typically used in combination with ipaddr_len to
            pass address and its length to POSIX socket APIs.
        `,

        example: ipaddr_example,
    }
)
