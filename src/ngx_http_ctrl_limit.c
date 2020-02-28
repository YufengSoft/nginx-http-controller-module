
/*
 * Copyright (C) hongzhidao
 */


#include <ngx_http_ctrl.h>


static ngx_rbtree_node_t *ngx_http_ctrl_limit_conn_lookup(ngx_rbtree_t *rbtree,
    ngx_str_t *key, uint32_t hash);
static ngx_int_t ngx_http_ctrl_limit_req_lookup(ngx_http_ctrl_shctx_t *shctx,
    ngx_http_action_limit_req_t *lr, ngx_str_t *key, ngx_uint_t hash,
    ngx_uint_t *ep);
static void ngx_http_ctrl_limit_req_expire(ngx_http_ctrl_shctx_t *shctx,
    ngx_http_action_limit_req_t *lr, ngx_uint_t n);
static void ngx_http_ctrl_limit_req_delay(ngx_http_request_t *r);


ngx_int_t
ngx_http_ctrl_limit_conn(ngx_http_request_t *r, ngx_http_action_limit_conn_t *lc)
{
    size_t                               size;
    uint32_t                             hash;
    ngx_str_t                            key;
    ngx_rbtree_node_t                   *node;
    ngx_http_ctrl_ctx_t                 *ctx;
    ngx_http_ctrl_shctx_t               *shctx;
    ngx_http_ctrl_main_conf_t           *cmcf;
    ngx_http_ctrl_limit_conn_node_t     *cn;

    ctx = ngx_http_get_module_ctx(r, ngx_http_ctrl_module);
    cmcf = ngx_http_get_module_main_conf(r, ngx_http_ctrl_module);
    shctx = cmcf->shm_zone->data;

    key.len = lc->key.length;
    key.data = lc->key.start;

    hash = ngx_crc32_short(key.data, key.len);

    ngx_shmtx_lock(&shctx->shpool->mutex);

    node = ngx_http_ctrl_limit_conn_lookup(&shctx->sh->limit_conn_rbtree, &key, hash);

    if (node == NULL) {
        size = offsetof(ngx_rbtree_node_t, color)
               + offsetof(ngx_http_ctrl_limit_conn_node_t, data)
               + key.len;

        node = ngx_slab_alloc_locked(shctx->shpool, size);

        if (node == NULL) {
            ngx_shmtx_unlock(&shctx->shpool->mutex);
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        cn = (ngx_http_ctrl_limit_conn_node_t *) &node->color;

        node->key = hash;
        cn->len = (u_char) key.len;
        cn->conn = 1;
        ngx_memcpy(cn->data, key.data, key.len);

        ngx_rbtree_insert(&shctx->sh->limit_conn_rbtree, node);

    } else {
        cn = (ngx_http_ctrl_limit_conn_node_t *) &node->color;

        if ((ngx_uint_t) cn->conn >= lc->conn) {
            ngx_shmtx_unlock(&shctx->shpool->mutex);
            return NGX_HTTP_FORBIDDEN;
        }

        cn->conn++;
    }

    ngx_shmtx_unlock(&shctx->shpool->mutex);

    ctx->node = node;

    return NGX_DECLINED;
}


static ngx_rbtree_node_t *
ngx_http_ctrl_limit_conn_lookup(ngx_rbtree_t *rbtree, ngx_str_t *key, uint32_t hash)
{
    ngx_int_t                         rc;
    ngx_rbtree_node_t                *node, *sentinel;
    ngx_http_ctrl_limit_conn_node_t  *cn;

    node = rbtree->root;
    sentinel = rbtree->sentinel;

    while (node != sentinel) {

        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        cn = (ngx_http_ctrl_limit_conn_node_t *) &node->color;

        rc = ngx_memn2cmp(key->data, cn->data, key->len, (size_t) cn->len);

        if (rc == 0) {
            return node;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    return NULL;
}


ngx_int_t
ngx_http_ctrl_limit_req(ngx_http_request_t *r, ngx_http_action_limit_req_t *lr)
{
    uint32_t                    hash;
    ngx_int_t                   rc;
    ngx_str_t                   key;
    nxt_uint_t                  delay;
    ngx_uint_t                  excess;
    ngx_http_ctrl_shctx_t      *shctx;
    ngx_http_ctrl_main_conf_t  *cmcf;

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_ctrl_module);
    shctx = cmcf->shm_zone->data;

    key.len = lr->key.length;
    key.data = lr->key.start;

    hash = ngx_crc32_short(key.data, key.len);

    ngx_shmtx_lock(&shctx->shpool->mutex);

    rc = ngx_http_ctrl_limit_req_lookup(shctx, lr, &key, hash, &excess);

    ngx_shmtx_unlock(&shctx->shpool->mutex);

    if (rc == NGX_BUSY || rc == NGX_ERROR) {
        return NGX_HTTP_SERVICE_UNAVAILABLE;
    }

    /* rc == NGX_OK */

    delay = excess * 1000 / lr->rate;

    if (!delay) {
        return NGX_DECLINED;
    }

    if (ngx_handle_read_event(r->connection->read, 0) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    r->read_event_handler = ngx_http_test_reading;
    r->write_event_handler = ngx_http_ctrl_limit_req_delay;

    r->connection->write->delayed = 1;
    ngx_add_timer(r->connection->write, delay);

    return NGX_AGAIN;
}


static ngx_int_t
ngx_http_ctrl_limit_req_lookup(ngx_http_ctrl_shctx_t *shctx,
    ngx_http_action_limit_req_t *lr, ngx_str_t *key, ngx_uint_t hash, ngx_uint_t *ep)
{
    size_t                             size;
    ngx_int_t                          rc, excess;
    ngx_msec_t                         now;
    ngx_msec_int_t                     ms;
    ngx_rbtree_node_t                 *node, *sentinel;
    ngx_http_ctrl_limit_req_node_t    *rn;

    now = ngx_current_msec;

    node = shctx->sh->limit_req_rbtree.root;
    sentinel = shctx->sh->limit_req_rbtree.sentinel;

    while (node != sentinel) {

        if (hash < node->key) {
            node = node->left;
            continue;
        }

        if (hash > node->key) {
            node = node->right;
            continue;
        }

        /* hash == node->key */

        rn = (ngx_http_ctrl_limit_req_node_t *) &node->color;

        rc = ngx_memn2cmp(key->data, rn->data, key->len, (size_t) rn->len);

        if (rc == 0) {
            ngx_queue_remove(&rn->queue);
            ngx_queue_insert_head(&shctx->sh->limit_req_queue, &rn->queue);

            ms = (ngx_msec_int_t) (now - rn->last);

            if (ms < -60000) {
                ms = 1;

            } else if (ms < 0) {
                ms = 0;
            }

            excess = rn->excess - lr->rate * ms / 1000 + 1000;

            if (excess < 0) {
                excess = 0;
            }

            *ep = excess;

            if ((ngx_uint_t) excess > lr->burst) {
                return NGX_BUSY;
            }

            rn->excess = excess;

            if (ms) {
                rn->last = now;
            }

            return NGX_OK;
        }

        node = (rc < 0) ? node->left : node->right;
    }

    *ep = 0;

    size = offsetof(ngx_rbtree_node_t, color)
           + offsetof(ngx_http_ctrl_limit_req_node_t, data)
           + key->len;

    ngx_http_ctrl_limit_req_expire(shctx, lr, 1);

    node = ngx_slab_alloc_locked(shctx->shpool, size);

    if (node == NULL) {
        ngx_http_ctrl_limit_req_expire(shctx, lr, 0);

        node = ngx_slab_alloc_locked(shctx->shpool, size);
        if (node == NULL) {
            ngx_log_error(NGX_LOG_ALERT, ngx_cycle->log, 0,
                          "could not allocate node%s", shctx->shpool->log_ctx);
            return NGX_ERROR;
        }
    }

    node->key = hash;

    rn = (ngx_http_ctrl_limit_req_node_t *) &node->color;

    rn->len = (u_short) key->len;
    rn->excess = 0;

    ngx_memcpy(rn->data, key->data, key->len);

    ngx_rbtree_insert(&shctx->sh->limit_req_rbtree, node);

    ngx_queue_insert_head(&shctx->sh->limit_req_queue, &rn->queue);

    rn->last = now;

    return NGX_OK;
}


static void
ngx_http_ctrl_limit_req_expire(ngx_http_ctrl_shctx_t *shctx,
    ngx_http_action_limit_req_t *lr, ngx_uint_t n)
{
    ngx_int_t                        excess;
    ngx_msec_t                       now;
    ngx_queue_t                     *q;
    ngx_msec_int_t                   ms;
    ngx_rbtree_node_t               *node;
    ngx_http_ctrl_limit_req_node_t  *rn;

    now = ngx_current_msec;

    /*
     * n == 1 deletes one or two zero rate entries
     * n == 0 deletes oldest entry by force
     *        and one or two zero rate entries
     */

    while (n < 3) {

        if (ngx_queue_empty(&shctx->sh->limit_req_queue)) {
            return;
        }

        q = ngx_queue_last(&shctx->sh->limit_req_queue);

        rn = ngx_queue_data(q, ngx_http_ctrl_limit_req_node_t, queue);

        if (n++ != 0) {

            ms = (ngx_msec_int_t) (now - rn->last);
            ms = ngx_abs(ms);

            if (ms < 60000) {
                return;
            }

            excess = rn->excess - lr->rate * ms / 1000;

            if (excess > 0) {
                return;
            }
        }

        ngx_queue_remove(q);

        node = (ngx_rbtree_node_t *)
                   ((u_char *) rn - offsetof(ngx_rbtree_node_t, color));

        ngx_rbtree_delete(&shctx->sh->limit_req_rbtree, node);

        ngx_slab_free_locked(shctx->shpool, node);
    }
}


static void
ngx_http_ctrl_limit_req_delay(ngx_http_request_t *r)
{
    ngx_event_t  *wev;

    ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "limit_req delay");

    wev = r->connection->write;

    if (wev->delayed) {

        if (ngx_handle_write_event(wev, 0) != NGX_OK) {
            ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        }

        return;
    }

    if (ngx_handle_read_event(r->connection->read, 0) != NGX_OK) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    r->read_event_handler = ngx_http_block_reading;
    r->write_event_handler = ngx_http_core_run_phases;

    ngx_http_core_run_phases(r);
}


void
ngx_http_ctrl_limit_conn_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t                **p;
    ngx_http_ctrl_limit_conn_node_t   *cn, *cnt;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            cn = (ngx_http_ctrl_limit_conn_node_t *) &node->color;
            cnt = (ngx_http_ctrl_limit_conn_node_t *) &temp->color;

            p = (ngx_memn2cmp(cn->data, cnt->data, cn->len, cnt->len) < 0)
                ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}


void
ngx_http_ctrl_limit_req_rbtree_insert_value(ngx_rbtree_node_t *temp,
    ngx_rbtree_node_t *node, ngx_rbtree_node_t *sentinel)
{
    ngx_rbtree_node_t               **p;
    ngx_http_ctrl_limit_req_node_t   *rn, *rnt;

    for ( ;; ) {

        if (node->key < temp->key) {

            p = &temp->left;

        } else if (node->key > temp->key) {

            p = &temp->right;

        } else { /* node->key == temp->key */

            rn = (ngx_http_ctrl_limit_req_node_t *) &node->color;
            rnt = (ngx_http_ctrl_limit_req_node_t *) &temp->color;

            p = (ngx_memn2cmp(rn->data, rnt->data, rn->len, rnt->len) < 0)
                ? &temp->left : &temp->right;
        }

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    ngx_rbt_red(node);
}
