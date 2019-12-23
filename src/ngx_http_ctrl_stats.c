
/*
 * Copyright (C) hongzhidao
 */


#include <ngx_http_ctrl.h>


static nxt_conf_value_t *ngx_http_ctrl_stats_stub(ngx_http_request_t *r,
    nxt_mp_t *mp);
static nxt_conf_value_t *ngx_http_ctrl_stats_status(ngx_http_request_t *r,
    nxt_mp_t *mp);


void
ngx_http_ctrl_stats_code(ngx_http_request_t *r)
{
    ngx_uint_t                     status;
    ngx_slab_pool_t               *shpool;
    ngx_http_ctrl_stats_t         *stats;
    ngx_http_ctrl_shctx_t         *shctx;
    ngx_http_ctrl_main_conf_t      *cmcf;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_ctrl_module);

    shctx = cmcf->shm_zone->data;
    stats = shctx->stats;
    shpool = (ngx_slab_pool_t *) cmcf->shm_zone->shm.addr;

    ngx_shmtx_lock(&shpool->mutex);

    status = r->headers_out.status;

    if (status >= 200 && status < 300) {
        stats->n2xx++;

    } else if (status >= 300 && status < 400) {
        stats->n3xx++;

    } else if (status >= 400 && status < 500) {
        stats->n4xx++;

    } else if (status >= 500) {
        stats->n5xx++;

    } else {
        stats->n1xx++;
    }

    stats->total++;

    ngx_shmtx_unlock(&shpool->mutex);
}


ngx_int_t
ngx_http_ctrl_stats_handler(ngx_http_request_t *r)
{
    size_t                    size;
    nxt_mp_t                 *mp;
    nxt_str_t                 path, body;
    nxt_conf_value_t         *value, *stats;
    nxt_conf_value_t         *stub, *status;
    ngx_http_ctrl_ctx_t      *ctx;
    nxt_conf_json_pretty_t    pretty;

    static nxt_str_t stub_str = nxt_string("stub");
    static nxt_str_t status_str = nxt_string("status");

    ctx = ngx_http_ctrl_get_ctx(r);
    if (ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    mp = ctx->mem_pool;

    stats = nxt_conf_create_object(mp, 2);
    if (nxt_slow_path(stats == NULL)) {
        return NGX_ERROR;
    }

    stub = ngx_http_ctrl_stats_stub(r, mp);
    if (nxt_slow_path(stub == NULL)) {
        return NGX_ERROR;
    }

    status = ngx_http_ctrl_stats_status(r, mp);
    if (nxt_slow_path(stub == NULL)) {
        return NGX_ERROR;
    }

    nxt_conf_set_member(stats, &stub_str, stub, 0);
    nxt_conf_set_member(stats, &status_str, status, 1);

    path.start = r->uri.data;
    path.length = r->uri.len;

    if (nxt_str_start(&path, "/stats", 6)
        && (path.length == 6 || path.start[6] == '/'))
    {
        if (path.length == 6) {
            path.length = 1;

        } else {
            path.length -= 6;
            path.start += 6;
        }
    }

    value = nxt_conf_get_path(stats, &path);
    if (value == NULL) {
        return NGX_HTTP_NOT_FOUND;
    }

    nxt_memzero(&pretty, sizeof(nxt_conf_json_pretty_t));

    size = nxt_conf_json_length(value, &pretty);

    body.start = nxt_mp_alloc(mp, size);
    if (nxt_slow_path(body.start == NULL)) {
        return NGX_ERROR;
    }

    nxt_memzero(&pretty, sizeof(nxt_conf_json_pretty_t));

    body.length = nxt_conf_json_print(body.start, value, &pretty)
                  - body.start;

    return ngx_http_ctrl_response(r, 200, &body);
}


static nxt_conf_value_t *
ngx_http_ctrl_stats_stub(ngx_http_request_t *r, nxt_mp_t *mp)
{
    nxt_conf_value_t         *value;

#if (NGX_STAT_STUB)

    value = nxt_conf_create_object(mp, 7);
    if (nxt_slow_path(value == NULL)) {
        return NULL;
    }

    ngx_atomic_int_t          ac, ap, hn, rq, rd, wr, wa;

    ac = *ngx_stat_active;
    ap = *ngx_stat_accepted;
    hn = *ngx_stat_handled;
    rq = *ngx_stat_requests;
    rd = *ngx_stat_reading;
    wr = *ngx_stat_writing;
    wa = *ngx_stat_waiting;

    static nxt_str_t  active_str = nxt_string("active");
    static nxt_str_t  accepted_str = nxt_string("accepted");
    static nxt_str_t  handled_str = nxt_string("handled");
    static nxt_str_t  requests_str = nxt_string("requests");
    static nxt_str_t  reading_str = nxt_string("reading");
    static nxt_str_t  writing_str = nxt_string("writing");
    static nxt_str_t  waiting_str = nxt_string("waiting");

    nxt_conf_set_member_integer(value, &active_str, ac, 0);
    nxt_conf_set_member_integer(value, &accepted_str, ap, 1);
    nxt_conf_set_member_integer(value, &handled_str, hn, 2);
    nxt_conf_set_member_integer(value, &requests_str, rq, 3);
    nxt_conf_set_member_integer(value, &reading_str, rd, 4);
    nxt_conf_set_member_integer(value, &writing_str, wr, 5);
    nxt_conf_set_member_integer(value, &waiting_str, wa, 6);

#else

    value = nxt_conf_create_object(mp, 0);
    if (nxt_slow_path(value == NULL)) {
        return NULL;
    }

#endif

    return value;
}


static nxt_conf_value_t *
ngx_http_ctrl_stats_status(ngx_http_request_t *r, nxt_mp_t *mp)
{
    ngx_slab_pool_t               *shpool;
    nxt_conf_value_t              *value;
    ngx_http_ctrl_stats_t         *stats;
    ngx_http_ctrl_shctx_t         *shctx;
    ngx_http_ctrl_loc_conf_t      *clcf;
    ngx_http_ctrl_main_conf_t     *cmcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_ctrl_module);
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_ctrl_module);

    shctx = cmcf->shm_zone->data;
    stats = shctx->stats;
    shpool = (ngx_slab_pool_t *) cmcf->shm_zone->shm.addr;

    value = nxt_conf_create_object(mp, 6);
    if (nxt_slow_path(value == NULL)) {
        return NULL;
    }

    static nxt_str_t  xx1_str = nxt_string("n1xx");
    static nxt_str_t  xx2_str = nxt_string("n2xx");
    static nxt_str_t  xx3_str = nxt_string("n3xx");
    static nxt_str_t  xx4_str = nxt_string("n4xx");
    static nxt_str_t  xx5_str = nxt_string("n5xx");
    static nxt_str_t  total_str = nxt_string("total");

    ngx_shmtx_lock(&shpool->mutex);

    nxt_conf_set_member_integer(value, &xx1_str, stats->n1xx, 0);
    nxt_conf_set_member_integer(value, &xx2_str, stats->n2xx, 1);
    nxt_conf_set_member_integer(value, &xx3_str, stats->n3xx, 2);
    nxt_conf_set_member_integer(value, &xx4_str, stats->n4xx, 3);
    nxt_conf_set_member_integer(value, &xx5_str, stats->n5xx, 4);
    nxt_conf_set_member_integer(value, &total_str, stats->total, 5);

    ngx_shmtx_unlock(&shpool->mutex);

    return value;
}
