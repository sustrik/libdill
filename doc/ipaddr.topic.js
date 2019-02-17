ipaddr_example = `
    ipaddr addr;
    ipaddr_remote(&addr, "www.example.org", 80, 0, -1);
    int s = socket(ipaddr_family(addr), SOCK_STREAM, 0);
    connect(s, ipaddr_sockaddr(&addr), ipaddr_len(&addr));
`

ipaddr_mode = `
    Mode specifies which kind of addresses should be returned. Possible
    values are:

    * **IPADDR_IPV4**: Get IPv4 address.
    * **IPADDR_IPV6**: Get IPv6 address.
    * **IPADDR_PREF_IPV4**: Get IPv4 address if possible, IPv6 address otherwise.
    * **IPADDR_PREF_IPV6**: Get IPv6 address if possible, IPv4 address otherwise.

    Setting the argument to zero invokes default behaviour, which, at the
    present, is **IPADDR_PREF_IPV4**. However, in the future when IPv6 becomes
    more common it may be switched to **IPADDR_PREF_IPV6**.
`

