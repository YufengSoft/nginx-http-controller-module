
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_HTTP_ROUTE_H_INCLUDED_
#define _NXT_HTTP_ROUTE_H_INCLUDED_


typedef struct nxt_http_routes_s    nxt_http_routes_t;


typedef struct {
    uint32_t                        items;
    nxt_http_name_value_t           *variable[0];
} nxt_http_action_variables_t;


typedef struct {
    uint32_t                        items;
    nxt_addr_pattern_t              addr_pattern[0];
} nxt_http_action_addr_t;


typedef struct {
    uint32_t                        items;
    nxt_http_name_value_t           *header[0];
} nxt_http_action_headers_t;


typedef struct {
    nxt_http_action_variables_t     *variables;
    nxt_http_action_addr_t          *blacklist;
    nxt_http_action_addr_t          *whitelist;
    nxt_http_action_headers_t       *add_headers;
} nxt_http_action_t;


nxt_http_routes_t *nxt_http_routes_create(nxt_mp_t *mp,
    nxt_conf_value_t *routes_conf);
nxt_http_action_t *nxt_http_route_action(nxt_http_request_t *r,
    nxt_http_routes_t *routes);


#define NXT_HTTP_ACTION_ERROR  ((nxt_http_action_t *) -1)


#endif /* _NXT_HTTP_ROUTE_H_INCLUDED_ */
