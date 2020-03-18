
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_UPSTREAM_H_INCLUDED_
#define _NXT_UPSTREAM_H_INCLUDED_


typedef struct {
    nxt_sockaddr_t                             *sockaddr;

    int32_t                                    current_weight;
    int32_t                                    effective_weight;
    int32_t                                    weight;
    int32_t                                    max_conns;
    int32_t                                    max_fails;
    int32_t                                    fail_timeout;
    int8_t                                     down;
} nxt_upstream_round_robin_server_t;


typedef struct {
    uint32_t                                   items;
    nxt_upstream_round_robin_server_t          server[0];
} nxt_upstream_round_robin_t;


typedef struct {
    nxt_upstream_round_robin_t                 *round_robin;
    nxt_str_t                                  zone_name;
} nxt_upstream_t;


typedef struct {
    uint32_t                                   items;
    nxt_upstream_t                             upstream[0];
} nxt_upstreams_t;


nxt_upstreams_t *nxt_upstreams_create(nxt_mp_t *mp, nxt_conf_value_t *upstreams_conf);


#endif /* _NXT_UPSTREAM_H_INCLUDED_ */
