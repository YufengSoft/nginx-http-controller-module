
/*
 * Copyright (C) Axel Duch
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_ADDR_H_INCLUDED_
#define _NXT_ADDR_H_INCLUDED_


enum {
    NXT_ADDR_ANY = 0,
    NXT_ADDR_RANGE,
    NXT_ADDR_EXACT,
    NXT_ADDR_CIDR,
};


enum {
    NXT_ADDR_PATTERN_CV_TYPE_ERROR = NXT_OK + 1,
    NXT_ADDR_PATTERN_LENGTH_ERROR,
    NXT_ADDR_PATTERN_FORMAT_ERROR,
    NXT_ADDR_PATTERN_RANGE_OVERLAP_ERROR,
    NXT_ADDR_PATTERN_CIDR_ERROR,
    NXT_ADDR_PATTERN_NO_IPv6_ERROR,
};


typedef struct {
    in_addr_t           start;
    in_addr_t           end;
} nxt_addr_range_t;


#if (NXT_INET6)
typedef struct {
    struct in6_addr     start;
    struct in6_addr     end;
} nxt_in6_addr_range_t;
#endif


typedef struct {
    uint8_t             match_type:2;
    uint8_t             addr_family;

    union {
        nxt_addr_range_t       v4;
#if (NXT_INET6)
        nxt_in6_addr_range_t   v6;
#endif
    } addr;
} nxt_addr_pattern_t;


NXT_EXPORT nxt_int_t nxt_addr_pattern_parse(nxt_mp_t *mp,
    nxt_addr_pattern_t *pattern, nxt_conf_value_t *cv);
NXT_EXPORT nxt_int_t nxt_addr_pattern_match(nxt_addr_pattern_t *p,
    nxt_sockaddr_t *sa);


#endif /* _NXT_ADDR_H_INCLUDED_ */
