fxs = [
    {
        name: "ipaddr_equal",
        section: "IP addresses",
        info: "compares two IP address",
        result: {
            type: "int",
            info: "1 if the arguments are equal, 0 otherwise.",
        },
        args: [
            {
                name: "addr1",
                type: "const struct ipaddr*",
                info: "First IP address.",
            },
            {
                name: "addr2",
                type: "const struct ipaddr*",
                info: "Second IP address.",
            },
            {
                name: "ignore_port",
                type: "int",
                info: "If set to zero addresses with different ports will be " +
                      "considered unequal. Otherwise, ports won't be taken " +
                      "into account.",
            },
        ],

        prologue: `
            This function returns 1 is the two supplied IP addresses are
            the same or zero otherwise.
        `,

        example: ipaddr_example,
    },
    {
        name: "ipaddr_family",
        section: "IP addresses",
        info: "returns family of the IP address",
        result: {
            type: "int",
            info: "IP family.",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
        ],

        prologue: `
            Returns family of the address, i.e.  either AF_INET for IPv4
            addresses or AF_INET6 for IPv6 addresses.
        `,

        example: ipaddr_example,
    },
    {
        name: "ipaddr_len",
        section: "IP addresses",
        info: "returns length of the address",
        result: {
            type: "int",
            info: "Length of the IP address.",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
        ],

        prologue: `
            Returns lenght of the address, in bytes. This function is typically
            used in combination with **ipaddr_sockaddr** to pass address and its
            length to POSIX socket APIs.
        `,

        example: ipaddr_example,
    },
    {
        name: "ipaddr_local",
        section: "IP addresses",
        info: "resolve the address of a local network interface",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "struct ipaddr*",
                info: "Out parameter, The IP address object.",
            },
            {
                name: "name",
                type: "const char*",
                info: "Name of the local network interface, such as \"eth0\", \"192.168.0.111\" or \"::1\".",
            },
            {
                name: "port",
                type: "int",
                info: "Port number. Valid values are 1-65535.",
            },
            {
                name: "mode",
                type: "int",
                info: "What kind of address to return. See above.",
            },
        ],

        prologue: trimrect(`
            Converts an IP address in human-readable format, or a name of a
            local network interface into an **ipaddr** structure.
        `) + "\n\n" + trimrect(ipaddr_mode),

        custom_errors: {
            ENODEV: "Local network interface with the specified name does not exist."
        },

        example: `
            struct ipaddr addr;
            ipaddr_local(&addr, "eth0", 5555, 0);
            int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
            bind(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
        `,
    },
    {
        name: "ipaddr_port",
        section: "IP addresses",
        info: "returns the port part of the address",
        result: {
            type: "int",
            info: "The port number.",
        },
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
        ],

        prologue: `
            Returns the port part of the address.
        `,

        example: `
            int port = ipaddr_port(&addr);
        `,
    },
    {
        name: "ipaddr_remote",
        section: "IP addresses",
        info: "resolve the address of a remote IP endpoint",
        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
        args: [
            {
                name: "addr",
                type: "struct ipaddr*",
                info: "Out parameter, The IP address object.",
            },
            {
                name: "name",
                type: "const char*",
                info: "Name of the remote IP endpoint, such as \"www.example.org\" or \"192.168.0.111\".",
            },
            {
                name: "port",
                type: "int",
                info: "Port number. Valid values are 1-65535.",
            },
            {
                name: "mode",
                type: "int",
                info: "What kind of address to return. See above.",
            },
        ],

        has_deadline: true,

        prologue: trimrect(`
            Converts an IP address in human-readable format, or a name of a
            remote host into an **ipaddr** structure.
        `) + "\n\n" + trimrect(ipaddr_mode),

        custom_errors: {
            "EADDRNOTAVAIL": "The name of the remote host cannot be resolved to an address of the specified type.",
        },

        example: ipaddr_example,
    },
    {
        name: "ipaddr_remotes",
        section: "IP addresses",
        info: "resolve the address of a remote IP endpoint",
        result: {
            type: "int",
            success: "number of IP addresses returned",
            error: "-1",
        },
        args: [
            {
                name: "addrs",
                type: "struct ipaddr*",
                info: "Out parameter. Array of resulting IP addresses.",
            },
            {
                name: "naddrs",
                type: "int",
                info: "Size of **addrs** array.",
            },
            {
                name: "name",
                type: "const char*",
                info: "Name of the remote IP endpoint, such as \"www.example.org\" or \"192.168.0.111\".",
            },
            {
                name: "port",
                type: "int",
                info: "Port number. Valid values are 1-65535.",
            },
            {
                name: "mode",
                type: "int",
                info: "What kind of address to return. See above.",
            },
        ],

        has_deadline: true,

        prologue: trimrect(`
            Converts an IP address in human-readable format, or a name of a
            remote host into an array of **ipaddr** structures. If there's no
            associated address, the function succeeds and returns zero.
        `) + "\n\n" + trimrect(ipaddr_mode),


        example: ipaddr_example,
    },
    {
        name: "ipaddr_setport",
        section: "IP addresses",
        info: "changes port number of the address",
        args: [
            {
                name: "addr",
                type: "const struct ipaddr*",
                info: "IP address object.",
            },
        ],

        prologue: `
            Changes port number of the address.
        `,

        example: `
            ipaddr_setport(&addr, 80);
        `,
    },
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
    },
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
    },
]
