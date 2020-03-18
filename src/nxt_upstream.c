
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>


static nxt_upstream_round_robin_t *
    nxt_upstream_round_robin_create(nxt_mp_t *mp, nxt_conf_value_t *upstream_conf);


typedef struct {
    nxt_str_t       address;
    nxt_uint_t      weight;
    nxt_uint_t      max_conns;
    nxt_uint_t      max_fails;
    nxt_uint_t      fail_timeout;
    uint8_t         down;
} nxt_upstream_round_robin_server_conf_t;


static nxt_conf_map_t  nxt_upstream_round_robin_server_conf[] = {
    {
        nxt_string("address"),
        NXT_CONF_MAP_STR,
        offsetof(nxt_upstream_round_robin_server_conf_t, address),
    },
    {
        nxt_string("weight"),
        NXT_CONF_MAP_INT32,
        offsetof(nxt_upstream_round_robin_server_conf_t, weight),
    },
    {
        nxt_string("max_conns"),
        NXT_CONF_MAP_INT32,
        offsetof(nxt_upstream_round_robin_server_conf_t, max_conns),
    },
    {
        nxt_string("max_fails"),
        NXT_CONF_MAP_INT32,
        offsetof(nxt_upstream_round_robin_server_conf_t, max_fails),
    },
    {
        nxt_string("fail_timeout"),
        NXT_CONF_MAP_INT32,
        offsetof(nxt_upstream_round_robin_server_conf_t, fail_timeout),
    },
    {
        nxt_string("down"),
        NXT_CONF_MAP_INT8,
        offsetof(nxt_upstream_round_robin_server_conf_t, down),
    },
};


nxt_upstreams_t *
nxt_upstreams_create(nxt_mp_t *mp, nxt_conf_value_t *upstreams_conf)
{
    size_t            size;
    uint32_t          i, n, next;
    nxt_str_t         name, *string;
    nxt_upstream_t    *upstream;
    nxt_upstreams_t   *upstreams;
    nxt_conf_value_t  *upcf;

    n = nxt_conf_object_members_count(upstreams_conf);
    size = sizeof(nxt_upstreams_t) + n * sizeof(nxt_upstream_t);

    upstreams = nxt_mp_zalloc(mp, size);
    if (nxt_slow_path(upstreams == NULL)) {
        return NULL;
    }

    upstreams->items = n;
    next = 0;

    for (i = 0; i < n; i++) {
        upstream = &upstreams->upstream[i];
        upcf = nxt_conf_next_object_member(upstreams_conf, &name, &next);

        string = nxt_str_dup(mp, &upstream->zone_name, &name);
        if (nxt_slow_path(string == NULL)) {
            return NULL;
        }

        upstream->round_robin = nxt_upstream_round_robin_create(mp, upcf);
        if (nxt_slow_path(upstream->round_robin == NULL)) {
            return NULL;
        }
    }

    return upstreams;
}


static nxt_upstream_round_robin_t *
nxt_upstream_round_robin_create(nxt_mp_t *mp, nxt_conf_value_t *upstream_conf)
{
    size_t                                  size;
    uint32_t                                i, n;
    nxt_sockaddr_t                          *sa;
    nxt_conf_value_t                        *srvcf;
    nxt_upstream_round_robin_t              *urr;
    nxt_upstream_round_robin_server_t       *srv;
    nxt_upstream_round_robin_server_conf_t  server_conf;

    n = nxt_conf_array_elements_count(upstream_conf);

    size = sizeof(nxt_upstream_round_robin_t)
           + n * sizeof(nxt_upstream_round_robin_server_t);

    urr = nxt_mp_zalloc(mp, size);
    if (nxt_slow_path(urr == NULL)) {
        return NULL;
    }

    urr->items = n;

    for (i = 0; i < n; i++) {
        srvcf = nxt_conf_get_array_element(upstream_conf, i);
        srv = &urr->server[i];

        server_conf.weight = 1;
        server_conf.max_conns = 0;
        server_conf.max_fails = 1;
        server_conf.fail_timeout = 10;
        server_conf.down = 0;

        nxt_conf_map_object(mp, srvcf, nxt_upstream_round_robin_server_conf,
                            nxt_nitems(nxt_upstream_round_robin_server_conf),
                            &server_conf);

        sa = nxt_sockaddr_parse(mp, &server_conf.address);
        if (nxt_slow_path(sa == NULL)) {
            return NULL;
        }

        sa->type = SOCK_STREAM;

        srv->sockaddr = sa;

        srv->weight = server_conf.weight;
        srv->max_conns = server_conf.max_conns;
        srv->max_fails = server_conf.max_fails;
        srv->fail_timeout = server_conf.fail_timeout;
        srv->down = server_conf.down;
    }

    return urr;
}
