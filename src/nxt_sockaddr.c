
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>


in_addr_t
nxt_inet_addr(u_char *buf, size_t length)
{
    u_char      c, *end;
    in_addr_t   addr;
    nxt_uint_t  digit, octet, dots;

    addr = 0;
    octet = 0;
    dots = 0;

    end = buf + length;

    while (buf < end) {

        c = *buf++;

        digit = c - '0';
        /* values below '0' become large unsigned integers */

        if (digit < 10) {
            octet = octet * 10 + digit;
            continue;
        }

        if (c == '.' && octet < 256) {
            addr = (addr << 8) + octet;
            octet = 0;
            dots++;
            continue;
        }

        return INADDR_NONE;
    }

    if (dots == 3 && octet < 256) {
        addr = (addr << 8) + octet;
        return htonl(addr);
    }

    return INADDR_NONE;
}


#if (NXT_INET6)

nxt_int_t
nxt_inet6_addr(struct in6_addr *in6_addr, u_char *buf, size_t length)
{
    u_char      c, *addr, *zero_start, *ipv4, *dst, *src, *end;
    nxt_uint_t  digit, group, nibbles, groups_left;

    if (length == 0) {
        return NXT_ERROR;
    }

    end = buf + length;

    if (buf[0] == ':') {
        buf++;
    }

    addr = in6_addr->s6_addr;
    zero_start = NULL;
    groups_left = 8;
    nibbles = 0;
    group = 0;
    ipv4 = NULL;

    while (buf < end) {
        c = *buf++;

        if (c == ':') {
            if (nibbles != 0) {
                ipv4 = buf;

                *addr++ = (u_char) (group >> 8);
                *addr++ = (u_char) (group & 0xFF);
                groups_left--;

                if (groups_left != 0) {
                    nibbles = 0;
                    group = 0;
                    continue;
                }

            } else {
                if (zero_start == NULL) {
                    ipv4 = buf;
                    zero_start = addr;
                    continue;
                }
            }

            return NXT_ERROR;
        }

        if (c == '.' && nibbles != 0) {

            if (groups_left < 2 || ipv4 == NULL) {
                return NXT_ERROR;
            }

            group = nxt_inet_addr(ipv4, end - ipv4);
            if (group == INADDR_NONE) {
                return NXT_ERROR;
            }

            group = ntohl(group);

            *addr++ = (u_char) ((group >> 24) & 0xFF);
            *addr++ = (u_char) ((group >> 16) & 0xFF);
            groups_left--;

            /* the low 16-bit are copied below */
            break;
        }

        nibbles++;

        if (nibbles > 4) {
            return NXT_ERROR;
        }

        group <<= 4;

        digit = c - '0';
        /* values below '0' become large unsigned integers */

        if (digit < 10) {
            group += digit;
            continue;
        }

        c |= 0x20;
        digit = c - 'a';
        /* values below 'a' become large unsigned integers */

        if (digit < 6) {
            group += 10 + digit;
            continue;
        }

        return NXT_ERROR;
    }

    if (nibbles == 0 && zero_start == NULL) {
        return NXT_ERROR;
    }

    *addr++ = (u_char) (group >> 8);
    *addr++ = (u_char) (group & 0xFF);
    groups_left--;

    if (groups_left != 0) {

        if (zero_start != NULL) {

            /* moving part before consecutive zero groups to the end */

            groups_left *= 2;
            src = addr - 1;
            dst = src + groups_left;

            while (src >= zero_start) {
                *dst-- = *src--;
            }

            nxt_memzero(zero_start, groups_left);

            return NXT_OK;
        }

    } else {
        if (zero_start == NULL) {
            return NXT_OK;
        }
    }

    return NXT_ERROR;
}

#endif
