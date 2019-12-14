
/*
 * Copyright (C) hongzhidao
 */


#include <ngx_http_ctrl.h>


static void ngx_http_ctrl_cleanup(void *data);
static ngx_int_t ngx_http_ctrl_init(ngx_conf_t *cf);
static void *ngx_http_ctrl_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_ctrl_merge_loc_conf(ngx_conf_t *cf, void *parent,
    void *child);
static char *ngx_http_ctrl_stats(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_ctrl_stats_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_ctrl_stats_display(ngx_conf_t *cf, ngx_command_t *cmd,void *conf);


static ngx_command_t  ngx_http_ctrl_commands[] = {

    { ngx_string("ctrl_stats"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_ctrl_stats,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("ctrl_stats_zone"),
      NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE1,
      ngx_http_ctrl_stats_zone,
      0,
      0,
      NULL },

    { ngx_string("ctrl_stats_display"),
      NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_ctrl_stats_display,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_ctrl_module_ctx = {
    NULL,                           /* preconfiguration */
    ngx_http_ctrl_init,             /* postconfiguration */

    NULL,                           /* create main configuration */
    NULL,                           /* init main configuration */

    NULL,                           /* create server configuration */
    NULL,                           /* merge server configuration */

    ngx_http_ctrl_create_loc_conf,  /* create location configuration */
    ngx_http_ctrl_merge_loc_conf    /* merge location configuration */
};


ngx_module_t  ngx_http_ctrl_module = {
    NGX_MODULE_V1,
    &ngx_http_ctrl_module_ctx,      /* module context */
    ngx_http_ctrl_commands,         /* module directives */
    NGX_HTTP_MODULE,                /* module type */
    NULL,                           /* init master */
    NULL,                           /* init module */
    NULL,                           /* init process */
    NULL,                           /* init thread */
    NULL,                           /* exit thread */
    NULL,                           /* exit process */
    NULL,                           /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;


static ngx_int_t
ngx_http_ctrl_header_filter(ngx_http_request_t *r)
{
    ngx_http_ctrl_loc_conf_t      *clcf;

    clcf = ngx_http_get_module_loc_conf(r, ngx_http_ctrl_module);

    if (clcf->shm_zone != NULL) {
        ngx_http_ctrl_stats_code(r, clcf->shm_zone);
    }

    return ngx_http_next_header_filter(r);
}


ngx_int_t
ngx_http_ctrl_init_ctx(ngx_http_request_t *r)
{
    nxt_mp_t                     *mp;
    ngx_pool_cleanup_t           *cln;
    ngx_http_ctrl_ctx_t          *ctx;

    if (r != r->main) {
        return NGX_OK;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_ctrl_module);

    if (ctx != NULL) {
        return NGX_OK;
    }

    mp = nxt_mp_create(4096, 128, 1024, 64);
    if (mp == NULL) {
        return NGX_ERROR;
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_ctrl_ctx_t));
    if (ctx == NULL) {
        goto fail;
    }

    ngx_http_set_ctx(r, ctx, ngx_http_ctrl_module);

    ctx->mem_pool = mp;

    cln = ngx_pool_cleanup_add(r->pool, 0);
    if (cln == NULL) {
        goto fail;
    }

    cln->handler = ngx_http_ctrl_cleanup;
    cln->data = ctx;

    return NGX_OK;

fail:

    nxt_mp_destroy(mp);

    return NGX_ERROR;
}


static void
ngx_http_ctrl_cleanup(void *data)
{
    ngx_http_ctrl_ctx_t *ctx = data;

    nxt_mp_destroy(ctx->mem_pool);
}


ngx_int_t
ngx_http_ctrl_response(ngx_http_request_t *r, nxt_uint_t status,
    nxt_str_t *body)
{
    ngx_int_t             rc;
    ngx_buf_t            *b;
    ngx_chain_t           out;

    b = ngx_pcalloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    b->pos = body->start;
    b->last = b->pos + body->length;
    b->memory = 1;
    b->last_buf = 1;

    r->headers_out.content_type.len = sizeof("text/plain") - 1;
    r->headers_out.content_type.data = (u_char *) "text/plain";

    r->headers_out.status = status;
    r->headers_out.content_length_n = b->last - b->pos;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    out.buf = b;
    out.next = NULL;

    return ngx_http_output_filter(r, &out);
}


static void *
ngx_http_ctrl_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_ctrl_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_ctrl_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->shm_zone = NULL;
     */

    return conf;
}


static char *
ngx_http_ctrl_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_ctrl_loc_conf_t *prev = parent;
    ngx_http_ctrl_loc_conf_t *conf = child;

    if (conf->shm_zone == NULL) {
        conf->shm_zone = prev->shm_zone;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_ctrl_stats(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_ctrl_loc_conf_t  *ctrl_conf = conf;

    ngx_str_t                 *value;
    ngx_shm_zone_t            *shm_zone;

    value = cf->args->elts;

    shm_zone = ngx_shared_memory_add(cf, &value[1], 0, &ngx_http_ctrl_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    ctrl_conf->shm_zone = shm_zone;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_ctrl_init_zone(ngx_shm_zone_t *shm_zone, void *data)
{
    ngx_http_ctrl_shctx_t  *octx = data;

    size_t                  len;
    ngx_slab_pool_t        *shpool;
    ngx_http_ctrl_shctx_t  *ctx;

    ctx = shm_zone->data;

    if (octx) {
        ctx->stats = octx->stats;

        return NGX_OK;
    }

    shpool = (ngx_slab_pool_t *) shm_zone->shm.addr;

    if (shm_zone->shm.exists) {
        ctx->stats = shpool->data;

        return NGX_OK;
    }

    ctx->stats = ngx_slab_alloc(shpool, sizeof(ngx_http_ctrl_stats_t));
    if (ctx->stats == NULL) {
        return NGX_ERROR;
    }

    shpool->data = ctx->stats;

    len = sizeof(" in ctrl_zone \"\"") + shm_zone->shm.name.len;

    shpool->log_ctx = ngx_slab_alloc(shpool, len);
    if (shpool->log_ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_sprintf(shpool->log_ctx, " in ctrl_zone \"%V\"%Z",
                &shm_zone->shm.name);

    return NGX_OK;
}


static char *
ngx_http_ctrl_stats_zone(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    u_char                   *p;
    ssize_t                   size;
    ngx_str_t                *value, name, s;
    ngx_uint_t                i;
    ngx_shm_zone_t           *shm_zone;
    ngx_http_ctrl_shctx_t    *ctx;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_ctrl_shctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    size = 0;
    name.len = 0;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strncmp(value[i].data, "zone=", 5) == 0) {

            name.data = value[i].data + 5;

            p = (u_char *) ngx_strchr(name.data, ':');

            if (p == NULL) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid zone size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            name.len = p - name.data;

            s.data = p + 1;
            s.len = value[i].data + value[i].len - s.data;

            size = ngx_parse_size(&s);

            if (size == NGX_ERROR) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "invalid zone size \"%V\"", &value[i]);
                return NGX_CONF_ERROR;
            }

            if (size < (ssize_t) (8 * ngx_pagesize)) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "zone \"%V\" is too small", &value[i]);
                return NGX_CONF_ERROR;
            }

            continue;
        }

        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid parameter \"%V\"", &value[i]);
        return NGX_CONF_ERROR;
    }

    if (name.len == 0) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "\"%V\" must have \"zone\" parameter",
                           &cmd->name);
        return NGX_CONF_ERROR;
    }

    shm_zone = ngx_shared_memory_add(cf, &name, size, &ngx_http_ctrl_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    if (shm_zone->data) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "duplicate zone \"%V\"", &name);
        return NGX_CONF_ERROR;
    }

    shm_zone->init = ngx_http_ctrl_init_zone;
    shm_zone->data = ctx;

    return NGX_CONF_OK;
}


static char *
ngx_http_ctrl_stats_display(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_ctrl_loc_conf_t  *ctrl_conf = conf;

    ngx_str_t                 *value;
    ngx_shm_zone_t            *shm_zone;
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

    value = cf->args->elts;

    shm_zone = ngx_shared_memory_add(cf, &value[1], 0, &ngx_http_ctrl_module);
    if (shm_zone == NULL) {
        return NGX_CONF_ERROR;
    }

    ctrl_conf->shm_zone = shm_zone;

    clcf->handler = ngx_http_ctrl_stats_handler;

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_ctrl_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_ctrl_header_filter;

    return NGX_OK;
}
