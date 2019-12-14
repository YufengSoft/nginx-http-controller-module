
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>


typedef struct {
    nxt_conf_value_t        *root;
    nxt_mp_t                *pool;
} nxt_controller_conf_t;


static nxt_int_t nxt_controller_conf_default(void);
static nxt_int_t nxt_controller_response(nxt_mp_t *mp, nxt_controller_response_t *resp);
static nxt_int_t nxt_controller_conf_print(nxt_mp_t *mp, nxt_conf_value_t *value,
    nxt_str_t *result);


static nxt_controller_conf_t   nxt_controller_conf;


nxt_int_t
nxt_controller_start(nxt_controller_init_t *init)
{
    size_t           size; 
    ssize_t          n;
    nxt_int_t        ret;
    nxt_str_t        *conf;
    nxt_file_t       *file;
    nxt_file_info_t  fi;

    file = init->file;
    conf = &init->conf;

    ret = nxt_file_open(file, NXT_FILE_RDWR, NXT_FILE_CREATE_OR_OPEN,
                        NXT_FILE_OWNER_ACCESS);

    if (ret == NXT_OK) {
        ret = nxt_file_info(file, &fi);

        if (nxt_fast_path(ret == NXT_OK && nxt_is_file(&fi))) {
            size = nxt_file_size(&fi);

            if (size == 0) {
                goto default_conf;
            }

            conf->length = (size_t) nxt_file_size(&fi);
            conf->start = nxt_malloc(conf->length);

            if (conf->start == NULL) {
                return NGX_ERROR;
            }

            n = nxt_file_read(file, conf->start, conf->length, 0);

            if (nxt_slow_path(n != (ssize_t) conf->length)) {
                nxt_free(conf->start);
                
                goto default_conf;
            }

            ret = nxt_controller_conf_set(conf->start, conf->length);

            if (nxt_slow_path(ret != NXT_OK)) {
                nxt_free(conf->start);

                goto default_conf; 
            }

            return NXT_OK;
        }
    }

default_conf:

    return nxt_controller_conf_default();
}


nxt_int_t
nxt_controller_conf_set(u_char *data, size_t len)
{
    u_char                 *start, *end;
    nxt_mp_t               *mp;
    nxt_int_t              ret;
    nxt_conf_value_t       *conf;
    nxt_conf_validation_t  vldt;

    mp = nxt_mp_create(1024, 128, 256, 32);
    if (nxt_slow_path(mp == NULL)) {
        return NXT_ERROR;
    }

    start = nxt_mp_nget(mp, len);
    if (nxt_slow_path(start == NULL)) {
        goto fail;
    }

    end = nxt_cpymem(start, data, len);

    conf = nxt_conf_json_parse(mp, start, end, NULL);
    if (nxt_slow_path(conf == NULL)) {
        goto fail;
    }

    nxt_memzero(&vldt, sizeof(nxt_conf_validation_t));

    vldt.pool = nxt_mp_create(1024, 128, 256, 32);
    if (nxt_slow_path(vldt.pool == NULL)) {
        goto fail;
    }

    vldt.conf = conf;

    ret = nxt_conf_validate(&vldt);

    if (nxt_slow_path(ret != NXT_OK)) {
        nxt_mp_destroy(vldt.pool);
        goto fail;
    }

    nxt_mp_destroy(vldt.pool);
    
    if (nxt_controller_conf.pool != NULL) {
        nxt_mp_destroy(nxt_controller_conf.pool);
    }

    nxt_controller_conf.root = conf;
    nxt_controller_conf.pool = mp;

    return NXT_OK;

fail:

    nxt_mp_destroy(mp);

    return NXT_ERROR;
}


static nxt_int_t
nxt_controller_conf_default(void)
{
    nxt_mp_t          *mp;
    nxt_conf_value_t  *conf;

    static const nxt_str_t json = nxt_string("{ \"routes\": [] }");

    mp = nxt_mp_create(1024, 128, 256, 32);
    if (nxt_slow_path(mp == NULL)) {
        return NXT_ERROR;
    }

    conf = nxt_conf_json_parse_str(mp, &json);
    if (nxt_slow_path(conf == NULL)) {
        return NXT_ERROR;
    }

    nxt_controller_conf.root = conf;
    nxt_controller_conf.pool = mp;

    return NXT_OK;
}


nxt_int_t
nxt_controller_process_config(nxt_controller_request_t *req, nxt_controller_response_t *resp)
{
    nxt_mp_t               *mp;
    nxt_int_t              rc;
    nxt_str_t              *path;
    nxt_conf_op_t          *ops;
    nxt_conf_value_t       *value;
    nxt_conf_validation_t  vldt;
    nxt_conf_json_error_t  error;

    static const nxt_str_t empty_obj = nxt_string("{}");

    ngx_memzero(resp, sizeof(nxt_controller_response_t));

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

        value = nxt_conf_get_path(nxt_controller_conf.root, path);

        if (value == NULL) {
            goto not_found;
        }

        resp->status = 200;
        resp->conf = value;

        return nxt_controller_response(req->mem_pool, resp);
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

            return nxt_controller_response(req->mem_pool, resp);
        }

        if (path->length != 1) {
            rc = nxt_conf_op_compile(req->mem_pool, &ops, nxt_controller_conf.root,
                                     path, value, 0);

            if (rc != NXT_CONF_OP_OK) {
                nxt_mp_destroy(mp);

                switch (rc) {
                case NXT_CONF_OP_NOT_FOUND:
                    goto not_found;

                case NXT_CONF_OP_NOT_ALLOWED:
                    goto not_allowed;
                }

                /* rc == NXT_CONF_OP_ERROR */
                goto alloc_fail;
            }

            value = nxt_conf_clone(mp, ops, nxt_controller_conf.root);

            if (nxt_slow_path(value == NULL)) {
                nxt_mp_destroy(mp);
                goto alloc_fail;
            }
        }

        nxt_memzero(&vldt, sizeof(nxt_conf_validation_t));

        vldt.conf = value;
        vldt.pool = req->mem_pool;

        rc = nxt_conf_validate(&vldt);

        if (nxt_slow_path(rc != NXT_OK)) {
            nxt_mp_destroy(mp);

            if (rc == NXT_DECLINED) {
                resp->detail = vldt.error;
                goto invalid_conf;
            }

            /* rc == NXT_ERROR */
            goto alloc_fail;
        }

        nxt_mp_destroy(nxt_controller_conf.pool);

        nxt_controller_conf.pool = mp;
        nxt_controller_conf.root = value;

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
            rc = nxt_conf_op_compile(req->mem_pool, &ops, nxt_controller_conf.root,
                                     path, NULL, 0);

            if (rc != NXT_OK) {
                if (rc == NXT_CONF_OP_NOT_FOUND) {
                    goto not_found;
                }

                /* rc == NXT_CONF_OP_ERROR */
                goto alloc_fail;
            }

            mp = nxt_mp_create(1024, 128, 256, 32);
            if (nxt_slow_path(mp == NULL)) {
                goto alloc_fail;
            }

            value = nxt_conf_clone(mp, ops, nxt_controller_conf.root);
        }

        if (nxt_slow_path(value == NULL)) {
            nxt_mp_destroy(mp);
            goto alloc_fail;
        }

        nxt_memzero(&vldt, sizeof(nxt_conf_validation_t));

        vldt.conf = value;
        vldt.pool = mp;

        rc = nxt_conf_validate(&vldt);

        if (nxt_slow_path(rc != NXT_OK)) {
            nxt_mp_destroy(mp);

            if (rc == NXT_DECLINED) {
                resp->detail = vldt.error;
                goto invalid_conf;
            }

            /* rc == NXT_ERROR */
            goto alloc_fail;
        }

        nxt_mp_destroy(nxt_controller_conf.pool);

        nxt_controller_conf.pool = mp;
        nxt_controller_conf.root = value;

        goto conf_done;
    }

not_allowed:

    resp->status = 405;
    resp->title = (u_char *) "Method isn't allowed.";
    resp->offset = -1;

    return nxt_controller_response(req->mem_pool, resp);

not_found:

    resp->status = 404;
    resp->title = (u_char *) "Value doesn't exist.";
    resp->offset = -1;

    return nxt_controller_response(req->mem_pool, resp);

invalid_conf:

    resp->status = 400;
    resp->title = (u_char *) "Invalid configuration.";
    resp->offset = -1;

    return nxt_controller_response(req->mem_pool, resp);

alloc_fail:

    resp->status = 500;
    resp->title = (u_char *) "Memory allocation failed.";
    resp->offset = -1;

    return nxt_controller_response(req->mem_pool, resp);

conf_done:

    resp->status = 200;
    resp->title = (u_char *) "Reconfiguration done.";

    return nxt_controller_response(req->mem_pool, resp);
}


static nxt_int_t
nxt_controller_response(nxt_mp_t *mp, nxt_controller_response_t *resp)
{
    nxt_str_t         str, *result;
    nxt_uint_t        n;
    nxt_conf_value_t  *value, *location;

    result = &resp->result;

    static nxt_str_t  success_str = nxt_string("success");
    static nxt_str_t  error_str = nxt_string("error");
    static nxt_str_t  detail_str = nxt_string("detail");
    static nxt_str_t  location_str = nxt_string("location");
    static nxt_str_t  offset_str = nxt_string("offset");
    static nxt_str_t  line_str = nxt_string("line");
    static nxt_str_t  column_str = nxt_string("column");

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

    return nxt_controller_conf_print(mp, value, result);
}


nxt_int_t
nxt_controller_conf_print(nxt_mp_t *mp, nxt_conf_value_t *value, nxt_str_t *result)
{
    nxt_conf_json_pretty_t  pretty;

    nxt_memzero(&pretty, sizeof(nxt_conf_json_pretty_t));

    result->start = nxt_mp_alloc(mp, nxt_conf_json_length(value, &pretty));
    if (result->start == NULL) {
        return NXT_ERROR;
    }

    nxt_memzero(&pretty, sizeof(nxt_conf_json_pretty_t));

    result->length = nxt_conf_json_print(result->start, value, &pretty)
                     - result->start;

    return NXT_OK;
}


nxt_int_t
nxt_controller_conf_root(nxt_mp_t *mp, nxt_str_t *result)
{
    return nxt_controller_conf_print(mp, nxt_controller_conf.root, result);
}
