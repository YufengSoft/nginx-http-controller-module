
/*
 * Copyright (C) hongzhidao
 */


#ifndef _NGX_HTTP_CTRL_H_INCLUDED_
#define _NGX_HTTP_CTRL_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <nxt_main.h>


typedef struct {
    ngx_shm_zone_t             *shm_zone;
} ngx_http_ctrl_main_conf_t;


typedef struct {
    ngx_flag_t                  stats_enable;
} ngx_http_ctrl_loc_conf_t;


typedef struct {
    nxt_mp_t                   *mem_pool;
} ngx_http_ctrl_ctx_t;


typedef struct {
    ngx_atomic_t                n1xx;
    ngx_atomic_t                n2xx;
    ngx_atomic_t                n3xx;
    ngx_atomic_t                n4xx;
    ngx_atomic_t                n5xx;
    ngx_atomic_t                total;
} ngx_http_ctrl_stats_t;


typedef struct {
    ngx_http_ctrl_stats_t      *stats;
} ngx_http_ctrl_shctx_t;


ngx_http_ctrl_ctx_t *ngx_http_ctrl_get_ctx(ngx_http_request_t *r);
void ngx_http_ctrl_stats_code(ngx_http_request_t *r);
ngx_int_t ngx_http_ctrl_stats_handler(ngx_http_request_t *r);
ngx_int_t ngx_http_ctrl_response(ngx_http_request_t *r,
    nxt_uint_t status, nxt_str_t *body);


extern ngx_module_t  ngx_http_ctrl_module;


#endif /* _NGX_HTTP_CTRL_H_INCLUDED_ */
