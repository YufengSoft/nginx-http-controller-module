
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
    nxt_uint_t                      status;
    nxt_conf_value_t                *conf;

    u_char                          *title;
    nxt_str_t                       detail;
    ssize_t                         offset;
    nxt_uint_t                      line;
    nxt_uint_t                      column;

    nxt_str_t                       json;
    nxt_str_t                       response;

    nxt_file_t                      *file;
} ngx_http_conf_init_t;


ngx_int_t ngx_http_conf_start(nxt_file_t *file);
ngx_int_t ngx_http_conf_apply(nxt_mp_t *mp, nxt_conf_value_t *conf);
ngx_http_action_t *ngx_http_conf_action(nxt_http_request_t *r,
    ngx_http_conf_t **http_conf);
void ngx_http_conf_release(ngx_http_conf_t *http_conf);
ngx_int_t ngx_http_conf_handle(nxt_http_request_t *r,
    ngx_http_conf_init_t *init);


#endif /* _NGX_HTTP_CONF_H_INCLUDED_ */
