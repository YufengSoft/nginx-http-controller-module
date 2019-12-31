
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>


static nxt_conf_value_t *nxt_http_conf_read(nxt_mp_t *mp, nxt_file_t *file);
static nxt_int_t nxt_http_response_init(nxt_conf_request_t *req,
    nxt_conf_response_t *resp);
static nxt_int_t nxt_http_conf_stringify(nxt_mp_t *mp, nxt_conf_value_t *value,
    nxt_str_t *str);
static nxt_int_t nxt_http_conf_store(nxt_conf_request_t *req,
    nxt_conf_response_t *resp);


static nxt_http_conf_t  *nxt_http_conf;


nxt_int_t
nxt_http_conf_start(nxt_file_t *file)
{
    nxt_mp_t          *mp;
    nxt_conf_value_t  *conf;

    mp = nxt_mp_create(1024, 128, 256, 32);
    if (nxt_slow_path(mp == NULL)) {
        return NXT_ERROR;
    }

    conf = nxt_http_conf_read(mp, file);
    if (nxt_slow_path(conf == NULL)) {
        goto fail;
    }

    if (nxt_http_conf_apply(mp, conf) != NXT_OK) {
        goto fail;
    }

    return NXT_OK;

fail:

    nxt_mp_destroy(mp);

    return NXT_ERROR;
}


static nxt_conf_value_t *
nxt_http_conf_read(nxt_mp_t *mp, nxt_file_t *file)
{
    size_t                 size;
    u_char                 *start;
    ssize_t                n;
    nxt_int_t              ret;
    nxt_file_info_t        fi;
    nxt_conf_value_t       *value;
    nxt_conf_validation_t  vldt;

    static const nxt_str_t json = nxt_string("{ \"routes\": [] }");

    ret = nxt_file_open(file, NXT_FILE_RDWR, NXT_FILE_CREATE_OR_OPEN,
                        NXT_FILE_DEFAULT_ACCESS);

    start = NULL;

    if (ret == NXT_OK) {
        ret = nxt_file_info(file, &fi);

        if (nxt_fast_path(ret == NXT_OK && nxt_is_file(&fi))) {
            size = nxt_file_size(&fi);

            if (size == 0) {
                goto invalid;
            }

            start = nxt_mp_nget(mp, size);
            if (nxt_slow_path(start == NULL)) {
                return NULL;
            }

            n = nxt_file_read(file, start, size, 0);

            if (n == (ssize_t) size) {

                value = nxt_conf_json_parse(mp, start, start + size, NULL);

                if (nxt_fast_path(value != NULL)) {

                    nxt_memzero(&vldt, sizeof(nxt_conf_validation_t));

                    vldt.pool = nxt_mp_create(1024, 128, 256, 32);
                    if (nxt_slow_path(vldt.pool == NULL)) {
                        return NULL;
                    }

                    vldt.conf = value;

                    ret = nxt_conf_validate(&vldt);

                    nxt_mp_destroy(vldt.pool);

                    if (nxt_slow_path(ret == NXT_ERROR)) {
                        return NULL;
                    }

                    if (nxt_fast_path(ret == NXT_OK)) {
                        return value;
                    }
                }
            }

            nxt_mp_free(mp, start);
        }
    }

invalid:

    return nxt_conf_json_parse(mp, json.start, json.start + json.length, NULL);
}


nxt_int_t
nxt_http_conf_apply(nxt_mp_t *mp, nxt_conf_value_t *conf)
{
    nxt_http_conf_t     *http_conf;
    nxt_conf_value_t    *routes_conf;
    nxt_http_routes_t   *routes;

    static nxt_str_t  routes_path = nxt_string("/routes");

    http_conf = nxt_mp_zalloc(mp, sizeof(nxt_http_conf_t));
    if (http_conf == NULL) {
        return NXT_ERROR;
    }

    http_conf->count = 1;
    http_conf->pool = mp;
    http_conf->root = conf;

    routes_conf = nxt_conf_get_path(conf, &routes_path);

    if (nxt_fast_path(routes_conf != NULL)) {
        routes = nxt_http_routes_create(mp, routes_conf);
        if (nxt_slow_path(routes == NULL)) {
            return NXT_ERROR;
        }

        http_conf->routes = routes;
    }

    if (nxt_http_conf != NULL) {
        nxt_http_conf_release(nxt_http_conf);
    }

    nxt_http_conf = http_conf;

    return NXT_OK;
}


nxt_http_action_t *
nxt_http_conf_action(nxt_http_request_t *r, nxt_http_conf_t **http_conf)
{
    if (nxt_http_conf == NULL) {
        return NULL;
    }

    *http_conf = nxt_http_conf;

    nxt_http_conf->count++;

    if (nxt_http_conf->routes != NULL) {
        return nxt_http_route_action(r, nxt_http_conf->routes);
    }

    return NULL;
}


void
nxt_http_conf_release(nxt_http_conf_t *http_conf)
{
    http_conf->count--;

    if (http_conf->count == 0) {
        nxt_mp_destroy(http_conf->pool);
    }
}


nxt_int_t
nxt_http_conf_handle(nxt_conf_request_t *req, nxt_conf_response_t *resp)
{
    nxt_mp_t               *mp;
    nxt_int_t              ret;
    nxt_str_t              *path;
    nxt_conf_op_t          *ops;
    nxt_conf_value_t       *value;
    nxt_conf_validation_t  vldt;
    nxt_conf_json_error_t  error;

    static const nxt_str_t empty_obj = nxt_string("{}");

    ngx_memzero(resp, sizeof(nxt_conf_response_t));

    path = &req->path;

    if (nxt_str_start(path, "/config", 7)
        && (path->length == 7 || path->start[7] == '/'))
    {
        if (path->length == 7) {
            path->length = 1;

        } else {
            path->length -= 7;
            path->start += 7;
        }
    }

    if (nxt_str_eq(&req->method, "GET", 3)) {

        value = nxt_conf_get_path(nxt_http_conf->root, path);

        if (value == NULL) {
            goto not_found;
        }

        resp->status = 200;
        resp->conf = value;

        return nxt_http_response_init(req, resp);
    }

    if (nxt_str_eq(&req->method, "PUT", 3)) {

        mp = nxt_mp_create(1024, 128, 256, 32);
        if (nxt_slow_path(mp == NULL)) {
            goto alloc_fail;
        }

        nxt_memzero(&error, sizeof(nxt_conf_json_error_t));

        value = nxt_conf_json_parse(mp, req->body.start,
                                    req->body.start + req->body.length, &error);

        if (value == NULL) {
            nxt_mp_destroy(mp);

            if (error.pos == NULL) {
                goto alloc_fail;
            }

            resp->status = 400;
            resp->title = (u_char *) "Invalid JSON.";
            resp->detail.length = nxt_strlen(error.detail);
            resp->detail.start = error.detail;
            resp->offset = error.pos - req->body.start;

            nxt_conf_json_position(req->body.start, error.pos,
                                   &resp->line, &resp->column);

            return nxt_http_response_init(req, resp);
        }

        if (path->length != 1) {
            ret = nxt_conf_op_compile(req->mem_pool, &ops, nxt_http_conf->root,
                                      path, value, 0);

            if (ret != NXT_CONF_OP_OK) {
                nxt_mp_destroy(mp);

                switch (ret) {
                case NXT_CONF_OP_NOT_FOUND:
                    goto not_found;

                case NXT_CONF_OP_NOT_ALLOWED:
                    goto not_allowed;
                }

                /* ret == NXT_CONF_OP_ERROR */
                goto alloc_fail;
            }

            value = nxt_conf_clone(mp, ops, nxt_http_conf->root);

            if (nxt_slow_path(value == NULL)) {
                nxt_mp_destroy(mp);
                goto alloc_fail;
            }
        }

        nxt_memzero(&vldt, sizeof(nxt_conf_validation_t));

        vldt.conf = value;
        vldt.pool = req->mem_pool;

        ret = nxt_conf_validate(&vldt);

        if (nxt_slow_path(ret != NXT_OK)) {
            nxt_mp_destroy(mp);

            if (ret == NXT_DECLINED) {
                resp->detail = vldt.error;
                goto invalid_conf;
            }

            /* ret == NXT_ERROR */
            goto alloc_fail;
        }

        goto conf_done;
    }

    if (nxt_str_eq(&req->method, "DELETE", 6)) {

        if (path->length == 1) {
            mp = nxt_mp_create(1024, 128, 256, 32);
            if (nxt_slow_path(mp == NULL)) {
                goto alloc_fail;
            }

            value = nxt_conf_json_parse_str(mp, &empty_obj);

        } else {
            ret = nxt_conf_op_compile(req->mem_pool, &ops, nxt_http_conf->root,
                                      path, NULL, 0);

            if (ret != NXT_OK) {
                if (ret == NXT_CONF_OP_NOT_FOUND) {
                    goto not_found;
                }

                /* ret == NXT_CONF_OP_ERROR */
                goto alloc_fail;
            }

            mp = nxt_mp_create(1024, 128, 256, 32);
            if (nxt_slow_path(mp == NULL)) {
                goto alloc_fail;
            }

            value = nxt_conf_clone(mp, ops, nxt_http_conf->root);
        }

        if (nxt_slow_path(value == NULL)) {
            nxt_mp_destroy(mp);
            goto alloc_fail;
        }

        nxt_memzero(&vldt, sizeof(nxt_conf_validation_t));

        vldt.conf = value;
        vldt.pool = mp;

        ret = nxt_conf_validate(&vldt);

        if (nxt_slow_path(ret != NXT_OK)) {
            nxt_mp_destroy(mp);

            if (ret == NXT_DECLINED) {
                resp->detail = vldt.error;
                goto invalid_conf;
            }

            /* ret == NXT_ERROR */
            goto alloc_fail;
        }

        goto conf_done;
    }

    not_allowed:

    resp->status = 405;
    resp->title = (u_char *) "Method isn't allowed.";
    resp->offset = -1;

    return nxt_http_response_init(req, resp);

not_found:

    resp->status = 404;
    resp->title = (u_char *) "Value doesn't exist.";
    resp->offset = -1;

    return nxt_http_response_init(req, resp);

invalid_conf:

    resp->status = 400;
    resp->title = (u_char *) "Invalid configuration.";
    resp->offset = -1;

    return nxt_http_response_init(req, resp);

alloc_fail:

    resp->status = 500;
    resp->title = (u_char *) "Memory allocation failed.";
    resp->offset = -1;

    return nxt_http_response_init(req, resp);

conf_done:

    ret = nxt_http_conf_apply(mp, value);

    if (nxt_fast_path(ret == NXT_OK)) {

        ret = nxt_http_conf_stringify(req->mem_pool, value, &resp->json);
        if (ret != NXT_OK) {
            return ret;
        }

        ret = nxt_http_conf_store(req, resp);
        if (ret != NXT_OK) {
            return ret;
        }

        resp->status = 200;
        resp->title = (u_char *) "Reconfiguration done.";

    } else {
        resp->status = 500;
        resp->title = (u_char *) "Conf apply failed.";
        resp->offset = -1;
    }

    return nxt_http_response_init(req, resp);
}


static nxt_int_t
nxt_http_response_init(nxt_conf_request_t *req, nxt_conf_response_t *resp)
{
    nxt_mp_t          *mp;
    nxt_str_t         str;
    nxt_uint_t        n;
    nxt_conf_value_t  *value, *location;

    static nxt_str_t  success_str = nxt_string("success");
    static nxt_str_t  error_str = nxt_string("error");
    static nxt_str_t  detail_str = nxt_string("detail");
    static nxt_str_t  location_str = nxt_string("location");
    static nxt_str_t  offset_str = nxt_string("offset");
    static nxt_str_t  line_str = nxt_string("line");
    static nxt_str_t  column_str = nxt_string("column");

    mp = req->mem_pool;
    value = resp->conf;

    if (value == NULL) {
        n = 1
            + (resp->detail.length != 0)
            + (resp->status >= 400 && resp->offset != -1);

        value = nxt_conf_create_object(mp, n);
        if (nxt_slow_path(value == NULL)) {
            return NXT_ERROR;
        }

        str.length = nxt_strlen(resp->title);
        str.start = resp->title;

        if (resp->status < 400) {
            nxt_conf_set_member_string(value, &success_str, &str, 0);

        } else {
            nxt_conf_set_member_string(value, &error_str, &str, 0);
        }

        n = 0;

        if (resp->detail.length != 0) {
            n++;

            nxt_conf_set_member_string(value, &detail_str, &resp->detail, n);
        }

        if (resp->status >= 400 && resp->offset != -1) {
            n++;

            location = nxt_conf_create_object(mp, resp->line != 0 ? 3 : 1);

            nxt_conf_set_member(value, &location_str, location, n);

            nxt_conf_set_member_integer(location, &offset_str, resp->offset, 0);

            if (resp->line != 0) {
                nxt_conf_set_member_integer(location, &line_str,
                                            resp->line, 1);

                nxt_conf_set_member_integer(location, &column_str,
                                            resp->column, 2);
            }
        }
    }

    return nxt_http_conf_stringify(mp, value, &resp->response);
}


static nxt_int_t
nxt_http_conf_stringify(nxt_mp_t *mp, nxt_conf_value_t *value,
    nxt_str_t *str)
{
    nxt_conf_json_pretty_t  pretty;

    nxt_memzero(&pretty, sizeof(nxt_conf_json_pretty_t));

    str->start = nxt_mp_alloc(mp, nxt_conf_json_length(value, &pretty));
    if (str->start == NULL) {
        return NXT_ERROR;
    }

    nxt_memzero(&pretty, sizeof(nxt_conf_json_pretty_t));

    str->length = nxt_conf_json_print(str->start, value, &pretty)
                  - str->start;

    return NXT_OK;
}


static nxt_int_t
nxt_http_conf_store(nxt_conf_request_t *req, nxt_conf_response_t *resp)
{
    ssize_t    n;

    if (req->file->fd == NXT_FILE_INVALID) {
        return NXT_OK;
    }

    if (ftruncate(req->file->fd, 0) == -1) {
        return NXT_ERROR;
    }

    n = nxt_file_write(req->file, resp->json.start, resp->json.length, 0);

    if (n != (ssize_t) resp->json.length) {
        return NXT_ERROR;
    }

    return NXT_OK;
}
