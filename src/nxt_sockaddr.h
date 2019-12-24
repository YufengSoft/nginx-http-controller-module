
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_SOCKADDR_H_INCLUDED_
#define _NXT_SOCKADDR_H_INCLUDED_


typedef struct {
    union {
        struct sockaddr           sockaddr;
        struct sockaddr_in        sockaddr_in;
#if (NXT_INET6)
        struct sockaddr_in6       sockaddr_in6;
#endif
    } u;
} nxt_sockaddr_t;


NXT_EXPORT in_addr_t nxt_inet_addr(u_char *buf, size_t len);
#if (NXT_INET6)
NXT_EXPORT nxt_int_t nxt_inet6_addr(struct in6_addr *in6_addr, u_char *buf,
    size_t len);
#endif


#endif /* _NXT_SOCKADDR_H_INCLUDED_ */
