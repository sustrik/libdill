
newFunction(
    {
        name: "ipaddr_str",
        section: "IP addresses",
        info: "convert address to a human-readable string",
        result: {
            type: "const char*",
            info: "The function returns **ipstr** argument, i.e.  pointer to the formatted string.",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
            {
                name: "buf",
                type: "char*",
                info: "Buffer to store the result in. It must be at least **IPADDR_MAXSTRLEN** bytes long.",
            },
        ],

        prologue: "Formats address as a human-readable string.",

        example: `
              char buf[IPADDR_MAXSTRLEN];
              ipaddr_str(&addr, buf);
        `,
    }
)
