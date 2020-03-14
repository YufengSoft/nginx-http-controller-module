
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_SOCKADDR_H_INCLUDED_
#define _NXT_SOCKADDR_H_INCLUDED_


typedef struct {
    /* Socket type: SOCKS_STREAM, SOCK_DGRAM, etc. */
    uint8_t                       type;
    /* Size of struct sockaddr. */
    uint8_t                       socklen;
    /*
     * Textual sockaddr representation, e.g.: "127.0.0.1:8000",
     * "[::1]:8000", and "unix:/path/to/socket".
     */
    uint8_t                       start;
    uint8_t                       length;
    /*
     * Textual address representation, e.g: "127.0.0.1", "::1",
     * and "unix:/path/to/socket".
     */
    uint8_t                       address_start;
    uint8_t                       address_length;
    /*
     * Textual port representation, e.g. "8000".
     * Port length is (start + length) - port_start.
     */
    uint8_t                       port_start;

    union {
        struct sockaddr           sockaddr;
        struct sockaddr_in        sockaddr_in;
#if (NXT_INET6)
        struct sockaddr_in6       sockaddr_in6;
#endif
#if (NXT_HAVE_UNIX_DOMAIN)
        struct sockaddr_un        sockaddr_un;
    } u;
#endif
} nxt_sockaddr_t;


NXT_EXPORT nxt_sockaddr_t *nxt_sockaddr_parse(nxt_mp_t *mp, nxt_str_t *addr);
NXT_EXPORT in_addr_t nxt_inet_addr(u_char *buf, size_t len);
#if (NXT_INET6)
NXT_EXPORT nxt_int_t nxt_inet6_addr(struct in6_addr *in6_addr, u_char *buf,
    size_t len);
#endif


#define NXT_INET_ADDR_STR_LEN     nxt_length("255.255.255.255:65535")

#define NXT_INET6_ADDR_STR_LEN                                                \
    nxt_length("[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:65535")


#define nxt_sockaddr_size(sa)                                                 \
    (offsetof(nxt_sockaddr_t, u) + sa->socklen + sa->length)
#define nxt_sockaddr_start(sa)    nxt_pointer_to(sa, (sa)->start)
#define nxt_sockaddr_address(sa)  nxt_pointer_to(sa, (sa)->address_start)


#endif /* _NXT_SOCKADDR_H_INCLUDED_ */
