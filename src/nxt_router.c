
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>


static nxt_router_conf_t  *nxt_router_conf;


nxt_int_t
nxt_router_conf_apply(u_char *data, size_t len)
{
    u_char             *start, *end;
    nxt_mp_t           *mp;
    nxt_conf_value_t   *conf, *routes_conf;
    nxt_router_conf_t  *router_conf;
    nxt_http_routes_t  *routes;

    static nxt_str_t  routes_path = nxt_string("/routes");

    mp = nxt_mp_create(1024, 128, 256, 32);
    if (nxt_slow_path(mp == NULL)) {
        return NXT_ERROR;
    }

    start = nxt_mp_nget(mp, len);
    if (nxt_slow_path(start == NULL)) {
        goto fail;
    }

    end = nxt_cpymem(start, data, len);

    conf = nxt_conf_json_parse(mp, start, end, NULL);
    if (conf == NULL) {
        goto fail;
    }

    router_conf = nxt_mp_zalloc(mp, sizeof(nxt_router_conf_t));
    if (router_conf == NULL) {
        goto fail;
    }

    router_conf->count = 1;
    router_conf->pool = mp;

    routes_conf = nxt_conf_get_path(conf, &routes_path);

    if (nxt_fast_path(routes_conf != NULL)) {
        routes = nxt_http_routes_create(mp, routes_conf);
        if (nxt_slow_path(routes == NULL)) {
            goto fail;
        }

        router_conf->routes = routes;
    }

    if (nxt_router_conf != NULL) {
        nxt_router_conf_release(nxt_router_conf);
    }

    nxt_router_conf = router_conf;

    return NXT_OK;

fail:

    nxt_mp_destroy(mp);

    return NXT_ERROR;
}


nxt_http_action_t *
nxt_router_http_action(nxt_http_request_t *r, nxt_router_conf_t **router_conf)
{
    if (nxt_router_conf == NULL) {
        return NULL;
    }

    *router_conf = nxt_router_conf;

    nxt_router_conf->count++;

    if (nxt_router_conf->routes != NULL) {
        return nxt_http_route_action(r, nxt_router_conf->routes);  
    }

    return NULL;
}


void
nxt_router_conf_release(nxt_router_conf_t *router_conf)
{
    nxt_router_conf->count--;

    if (nxt_router_conf->count == 0) {
        nxt_mp_destroy(nxt_router_conf->pool);
    } 
}
