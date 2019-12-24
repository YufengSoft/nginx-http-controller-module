
/*
 * Copyright (C) Axel Duch
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>


static nxt_bool_t nxt_str_looks_like_ipv6(const nxt_str_t *str);
#if (NXT_INET6)
static nxt_bool_t nxt_valid_ipv6_blocks(u_char *c, size_t len);
#endif


nxt_int_t
nxt_http_addr_pattern_parse(nxt_mp_t *mp, nxt_http_addr_pattern_t *pattern,
    nxt_conf_value_t *cv)
{
    u_char                 *delim;
    nxt_int_t              ret, cidr_prefix;
    nxt_str_t              addr;
    nxt_http_addr_base_t   *base;
    nxt_http_addr_range_t  *inet;

    if (nxt_conf_type(cv) != NXT_CONF_STRING) {
        return NXT_ADDR_PATTERN_CV_TYPE_ERROR;
    }

    nxt_conf_get_string(cv, &addr);

    base = &pattern->base;

    if (addr.length > 0 && addr.start[0] == '!') {
        addr.start++;
        addr.length--;

        base->negative = 1;

    } else {
        base->negative = 0;
    }

    if (nxt_slow_path(addr.length < 2)) {
        return NXT_ADDR_PATTERN_LENGTH_ERROR;
    }

    if (nxt_str_looks_like_ipv6(&addr)) {
#if (NXT_INET6)
        uint8_t                    i;
        nxt_int_t                  len;
        nxt_http_in6_addr_range_t  *inet6;

        base->addr_family = AF_INET6;

        if (addr.start[0] == '[') {

            if (addr.length < 3
                || addr.start[addr.length - 1] != ']')
            {
                return NXT_ADDR_PATTERN_FORMAT_ERROR;
            }

            addr.start++;
            addr.length -= 2;
        }

        inet6 = &pattern->addr.v6;

        delim = nxt_memchr(addr.start, '-', addr.length);
        if (delim != NULL) {
            len = delim - addr.start;
            if (nxt_slow_path(!nxt_valid_ipv6_blocks(addr.start, len))) {
                return NXT_ADDR_PATTERN_FORMAT_ERROR;
            }

            ret = nxt_inet6_addr(&inet6->start, addr.start, len);
            if (nxt_slow_path(ret != NXT_OK)) {
                return NXT_ADDR_PATTERN_FORMAT_ERROR;
            }

            len = addr.start + addr.length - delim - 1;
            if (nxt_slow_path(!nxt_valid_ipv6_blocks(delim + 1, len))) {
                return NXT_ADDR_PATTERN_FORMAT_ERROR;
            }

            ret = nxt_inet6_addr(&inet6->end, delim + 1, len);
            if (nxt_slow_path(ret != NXT_OK)) {
                return NXT_ADDR_PATTERN_FORMAT_ERROR;
            }

            if (nxt_slow_path(nxt_memcmp(&inet6->start, &inet6->end,
                                         sizeof(struct in6_addr)) > 0))
            {
                return NXT_ADDR_PATTERN_RANGE_OVERLAP_ERROR;
            }

            base->match_type = NXT_HTTP_ADDR_RANGE;

            return NXT_OK;
        }

        delim = nxt_memchr(addr.start, '/', addr.length);
        if (delim != NULL) {
            cidr_prefix = nxt_int_parse(delim + 1,
                                        addr.start + addr.length - (delim + 1));
            if (nxt_slow_path(cidr_prefix < 0 || cidr_prefix > 128)) {
                return NXT_ADDR_PATTERN_CIDR_ERROR;
            }

            addr.length = delim - addr.start;
            if (nxt_slow_path(!nxt_valid_ipv6_blocks(addr.start,
                                                     addr.length)))
            {
                return NXT_ADDR_PATTERN_FORMAT_ERROR;
            }

            ret = nxt_inet6_addr(&inet6->start, addr.start, addr.length);
            if (nxt_slow_path(ret != NXT_OK)) {
                return NXT_ADDR_PATTERN_FORMAT_ERROR;
            }

            if (nxt_slow_path(cidr_prefix == 0)) {
                base->match_type = NXT_HTTP_ADDR_ANY;

                return NXT_OK;
            }

            if (nxt_slow_path(cidr_prefix == 128)) {
                base->match_type = NXT_HTTP_ADDR_EXACT;

                return NXT_OK;
            }

            base->match_type = NXT_HTTP_ADDR_CIDR;

            for (i = 0; i < sizeof(struct in6_addr); i++) {
                if (cidr_prefix >= 8) {
                    inet6->end.s6_addr[i] = 0xFF;
                    cidr_prefix -= 8;

                    continue;
                }

                if (cidr_prefix > 0) {
                    inet6->end.s6_addr[i] = 0xFF & (0xFF << (8 - cidr_prefix));
                    inet6->start.s6_addr[i] &= inet6->end.s6_addr[i];
                    cidr_prefix = 0;

                    continue;
                }

                inet6->start.s6_addr[i] = 0;
                inet6->end.s6_addr[i] = 0;
            }

            return NXT_OK;
        }

        base->match_type = NXT_HTTP_ADDR_EXACT;

        if (nxt_slow_path(!nxt_valid_ipv6_blocks(addr.start, addr.length))) {
            return NXT_ADDR_PATTERN_FORMAT_ERROR;
        }

        ret = nxt_inet6_addr(&inet6->start, addr.start, addr.length);
        if (nxt_slow_path(ret != NXT_OK)) {
            return NXT_ADDR_PATTERN_FORMAT_ERROR;
        }

        return NXT_OK;
#endif

        return NXT_ADDR_PATTERN_NO_IPv6_ERROR;
    }

    base->addr_family = AF_INET;
    
    inet = &pattern->addr.v4;

    delim = nxt_memchr(addr.start, '-', addr.length);

    if (delim != NULL) {
        inet->start = nxt_inet_addr(addr.start, delim - addr.start);
        if (nxt_slow_path(inet->start == INADDR_NONE)) {
            return NXT_ADDR_PATTERN_FORMAT_ERROR;
        }

        inet->end = nxt_inet_addr(delim + 1,
                                  addr.start + addr.length - (delim + 1));
        if (nxt_slow_path(inet->end == INADDR_NONE)) {
            return NXT_ADDR_PATTERN_FORMAT_ERROR;
        }

        if (nxt_slow_path(nxt_memcmp(&inet->start, &inet->end,
                                     sizeof(struct in_addr)) > 0))
        {
            return NXT_ADDR_PATTERN_RANGE_OVERLAP_ERROR;
        }

        base->match_type = NXT_HTTP_ADDR_RANGE;

        return NXT_OK;
    }

    delim = nxt_memchr(addr.start, '/', addr.length);
    if (delim != NULL) {
        cidr_prefix = nxt_int_parse(delim + 1,
                                    addr.start + addr.length - (delim + 1));
        if (nxt_slow_path(cidr_prefix < 0 || cidr_prefix > 32)) {
            return NXT_ADDR_PATTERN_CIDR_ERROR;
        }

        addr.length = delim - addr.start;
        inet->end = htonl(0xFFFFFFFF & (0xFFFFFFFF << (32 - cidr_prefix)));

        inet->start = nxt_inet_addr(addr.start, addr.length) & inet->end;
        if (nxt_slow_path(inet->start == INADDR_NONE)) {
            return NXT_ADDR_PATTERN_FORMAT_ERROR;
        }

        if (cidr_prefix == 0) {
            base->match_type = NXT_HTTP_ADDR_ANY;

        } else if (cidr_prefix < 32) {
            base->match_type = NXT_HTTP_ADDR_CIDR;
        }

        return NXT_OK;
    }

    inet->start = nxt_inet_addr(addr.start, addr.length);
    if (nxt_slow_path(inet->start == INADDR_NONE)) {
        return NXT_ADDR_PATTERN_FORMAT_ERROR;
    }

    base->match_type = NXT_HTTP_ADDR_EXACT;

    return NXT_OK;
}


static nxt_bool_t
nxt_str_looks_like_ipv6(const nxt_str_t *str)
{
    u_char  *colon, *end;

    colon = nxt_memchr(str->start, ':', str->length);

    if (colon != NULL) {
        end = str->start + str->length;
        colon = nxt_memchr(colon + 1, ':', end - (colon + 1));
    }

    return (colon != NULL);
}


#if (NXT_INET6)

static nxt_bool_t
nxt_valid_ipv6_blocks(u_char *c, size_t len)
{
    u_char      *end;
    nxt_uint_t  colon_gap;

    end = c + len;
    colon_gap = 0;

    while (c != end) {
        if (*c == ':') {
            colon_gap = 0;
            c++;

            continue;
        }

        colon_gap++;
        c++;

        if (nxt_slow_path(colon_gap > 4)) {
            return 0;
        }
    }

    return 1;
}

#endif


nxt_int_t
nxt_http_addr_pattern_match(nxt_http_addr_pattern_t *p, nxt_sockaddr_t *sa)
{
#if (NXT_INET6)
    uint32_t              i;
#endif
    nxt_int_t             match;
    struct sockaddr_in    *sin;
#if (NXT_INET6)
    struct sockaddr_in6   *sin6;
#endif
    nxt_http_addr_base_t  *base;

    base = &p->base;

    switch (sa->u.sockaddr.sa_family) {

    case AF_INET:

        match = (base->addr_family == AF_INET
                 || base->addr_family == AF_UNSPEC);
        if (!match) {
            break;
        }

        sin = &sa->u.sockaddr_in;

        switch (base->match_type) {

        case NXT_HTTP_ADDR_ANY:
            break;

        case NXT_HTTP_ADDR_EXACT:
            match = (nxt_memcmp(&sin->sin_addr, &p->addr.v4.start,
                                sizeof(struct in_addr))
                     == 0);
            break;

        case NXT_HTTP_ADDR_RANGE:
            match = (nxt_memcmp(&sin->sin_addr, &p->addr.v4.start,
                                sizeof(struct in_addr)) >= 0
                     && nxt_memcmp(&sin->sin_addr, &p->addr.v4.end,
                                   sizeof(struct in_addr)) <= 0);

        case NXT_HTTP_ADDR_CIDR:
            match = ((sin->sin_addr.s_addr & p->addr.v4.end)
                     == p->addr.v4.start);
            break;

        default:
            nxt_unreachable();
        }

        break;

#if (NXT_INET6)
    case AF_INET6:

        match = (base->addr_family == AF_INET6
                 || base->addr_family == AF_UNSPEC);
        if (!match) {
            break;
        }

        sin6 = &sa->u.sockaddr_in6;

        switch (base->match_type) {

        case NXT_HTTP_ADDR_ANY:
            break;

        case NXT_HTTP_ADDR_EXACT:
            match = (nxt_memcmp(&sin6->sin6_addr, &p->addr.v6.start,
                                sizeof(struct in6_addr))
                     == 0);
            break;

        case NXT_HTTP_ADDR_RANGE:
            match = (nxt_memcmp(&sin6->sin6_addr, &p->addr.v6.start,
                                sizeof(struct in6_addr)) >= 0
                     && nxt_memcmp(&sin6->sin6_addr, &p->addr.v6.end,
                                   sizeof(struct in6_addr)) <= 0);
            break;

        case NXT_HTTP_ADDR_CIDR:
            for (i = 0; i < 16; i++) {
                match = ((sin6->sin6_addr.s6_addr[i]
                          & p->addr.v6.end.s6_addr[i])
                         == p->addr.v6.start.s6_addr[i]);

                if (!match) {
                    break;
                }
            }

            break;

        default:
            nxt_unreachable();
        }

        break;
#endif

    default:
        match = 0;
        break;

    }

    return match ^ base->negative;
}
