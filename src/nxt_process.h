
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_PROCESS_H_INCLUDED_
#define _NXT_PROCESS_H_INCLUDED_


typedef struct {
    nxt_mp_t                        *mem_pool;

    nxt_str_t                       method;
    nxt_str_t                       path;
    nxt_str_t                       body;

    nxt_file_t                      *file;
} nxt_process_request_t;


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
} nxt_process_response_t;


typedef struct {
    uint32_t                        count;
    nxt_mp_t                        *pool;
    nxt_conf_value_t                *root;
    nxt_http_routes_t               *routes;
} nxt_process_conf_t;


nxt_int_t nxt_process_start(nxt_file_t *file);
nxt_int_t nxt_process_conf_apply(nxt_mp_t *mp, nxt_conf_value_t *conf);
nxt_http_action_t *nxt_process_http_action(nxt_http_request_t *r,
    nxt_process_conf_t **process_conf);
void nxt_process_conf_release(nxt_process_conf_t *process_conf);
nxt_int_t nxt_process_config_handle(nxt_process_request_t *req,
    nxt_process_response_t *resp);


#endif /* _NXT_PROCESS_H_INCLUDED_ */
