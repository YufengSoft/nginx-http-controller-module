
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NGX_HTTP_ROUTE_H_INCLUDED_
#define _NGX_HTTP_ROUTE_H_INCLUDED_


typedef struct ngx_http_routes_s    ngx_http_routes_t;


typedef struct {
    uint16_t                        hash;
    uint16_t                        name_length;
    uint32_t                        value_length;
    u_char                          *name;
    u_char                          *value;
} ngx_http_name_value_t;


typedef struct {
    uint32_t                        items;
    ngx_http_name_value_t           *variable[0];
} ngx_http_action_variables_t;


typedef struct {
    uint32_t                        items;
    nxt_addr_pattern_t              addr_pattern[0];
} ngx_http_action_addr_t;


typedef struct {
    uint32_t                        items;
    ngx_http_name_value_t           *header[0];
} ngx_http_action_headers_t;


typedef struct {
    nxt_str_t                       key;
    nxt_uint_t                      conn;
} ngx_http_action_limit_conn_t;


typedef struct {
    nxt_str_t                       key;
    nxt_uint_t                      rate;
    nxt_uint_t                      burst;
} ngx_http_action_limit_req_t;


typedef struct {
    ngx_http_action_variables_t     *variables;
    ngx_http_action_addr_t          *blacklist;
    ngx_http_action_addr_t          *whitelist;
    ngx_http_action_headers_t       *add_headers;
    ngx_http_action_limit_conn_t    *limit_conn;
    ngx_http_action_limit_req_t     *limit_req;
} ngx_http_action_t;


ngx_http_routes_t *ngx_http_routes_create(nxt_mp_t *mp,
    nxt_conf_value_t *routes_conf);
ngx_http_action_t *ngx_http_route_action(ngx_http_request_t *r,
    ngx_http_routes_t *routes);


#define NGX_HTTP_ACTION_ERROR  ((ngx_http_action_t *) -1)


#endif /* _NGX_HTTP_ROUTE_H_INCLUDED_ */
