
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NGX_HTTP_CONF_H_INCLUDED_
#define _NGX_HTTP_CONF_H_INCLUDED_


typedef struct {
    uint32_t                        count;
    nxt_mp_t                        *pool;
    nxt_conf_value_t                *root;
    ngx_http_routes_t               *routes;
} ngx_http_conf_t;


typedef struct {
    nxt_mp_t                        *mem_pool;
    nxt_str_t                       body;

    nxt_uint_t                      status;
    nxt_conf_value_t                *conf;

    u_char                          *title;
    nxt_str_t                       detail;
    ssize_t                         offset;
    nxt_uint_t                      line;
    nxt_uint_t                      column;

    nxt_str_t                       json;
    nxt_str_t                       resp;

    nxt_file_t                      *file;
} nxt_http_request_t;


ngx_int_t ngx_http_conf_start(nxt_file_t *file, nxt_str_t *error);
ngx_int_t ngx_http_conf_apply(nxt_mp_t *mp, nxt_conf_value_t *conf);
ngx_http_action_t *ngx_http_conf_action(ngx_http_request_t *r,
    ngx_http_conf_t **http_conf);
void ngx_http_conf_release(ngx_http_conf_t *http_conf);
ngx_int_t ngx_http_conf_handle(ngx_http_request_t *r,
    nxt_http_request_t *req);


#endif /* _NGX_HTTP_CONF_H_INCLUDED_ */
