
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
    ngx_uint_t                  nfd;
    ngx_socket_t               *write_fd;
    ngx_socket_t               *read_fd;
    ngx_connection_t          **conn;
    ngx_buf_t                  *buf;

    ngx_str_t                   state;
    nxt_file_t                  file;

    ngx_shm_zone_t             *shm_zone;
} ngx_http_ctrl_main_conf_t;


typedef struct {
    ngx_flag_t                  conf_enable;
    ngx_flag_t                  stats_enable;
} ngx_http_ctrl_loc_conf_t;


typedef struct {
    nxt_mp_t                   *mem_pool;

    nxt_http_request_t         *req;
    nxt_http_action_t          *action;
    nxt_http_conf_t            *http_conf;
} ngx_http_ctrl_ctx_t;


typedef struct {
    nxt_uint_t                  counter;
    ngx_str_t                   json;
} ngx_http_ctrl_conf_t;


typedef struct {
    ngx_atomic_t                n1xx;
    ngx_atomic_t                n2xx;
    ngx_atomic_t                n3xx;
    ngx_atomic_t                n4xx;
    ngx_atomic_t                n5xx;
    ngx_atomic_t                total;
} ngx_http_ctrl_stats_t;


typedef struct {
    ngx_http_ctrl_conf_t       *conf;
    ngx_http_ctrl_stats_t      *stats;
} ngx_http_ctrl_shctx_t;


typedef enum {
    NXT_PORT_MSG_CONF = 0,
} nxt_port_msg_type_t;


typedef struct {
    size_t                      size;
    nxt_port_msg_type_t         type;
} nxt_port_msg_t;


ngx_http_ctrl_ctx_t *ngx_http_ctrl_get_ctx(ngx_http_request_t *r);
ngx_int_t ngx_http_ctrl_request_init(ngx_http_request_t *r);
ngx_int_t ngx_http_ctrl_set_variables(ngx_http_request_t *r,
    nxt_http_action_variables_t *variables);
ngx_int_t ngx_http_ctrl_blacklist(ngx_http_request_t *r,
    nxt_http_action_addr_t *blacklist);
ngx_int_t ngx_http_ctrl_whitelist(ngx_http_request_t *r,
    nxt_http_action_addr_t *whitelist);
void ngx_http_ctrl_stats_code(ngx_http_request_t *r);
ngx_int_t ngx_http_ctrl_stats_handler(ngx_http_request_t *r);
ngx_int_t ngx_http_ctrl_response(ngx_http_request_t *r,
    nxt_uint_t status, nxt_str_t *body);

ngx_int_t ngx_http_ctrl_config_handler(ngx_http_request_t *r);
void ngx_http_ctrl_notify_write_handler(ngx_event_t *rev);
void ngx_http_ctrl_notify_read_handler(ngx_event_t *rev);


extern ngx_module_t  ngx_http_ctrl_module;


#endif /* _NGX_HTTP_CTRL_H_INCLUDED_ */
