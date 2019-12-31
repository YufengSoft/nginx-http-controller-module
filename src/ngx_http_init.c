
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>
#include <ngx_http_init.h>


static ngx_http_name_value_t *ngx_http_argument(nxt_array_t *array,
    u_char *name, size_t name_length, uint32_t hash, u_char *start,
    u_char *end);


nxt_array_t *
ngx_http_arguments_parse(nxt_http_request_t *r)
{
    size_t                 name_length;
    u_char                 c, *p, *start, *end, *name;
    uint32_t               hash;
    nxt_bool_t             valid;
    nxt_array_t            *args;
    ngx_http_name_value_t  *nv;

    if (r->arguments != NULL) {
        return r->arguments;
    }

    args = nxt_array_create(r->mem_pool, 2, sizeof(ngx_http_name_value_t));
    if (nxt_slow_path(args == NULL)) {
        return NULL;
    }

    hash = NGX_HTTP_FIELD_HASH_INIT;
    valid = 1;
    name = NULL;
    name_length = 0;

    start = r->args.start;
    end = start + r->args.length;

    for (p = start; p < end; p++) {
        c = *p;

        if (c == '=') {
            name_length = p - start;
            name = start;
            start = p + 1;
            valid = (name_length != 0);

        } else if (c == '&') {
            if (valid) {
                nv = ngx_http_argument(args, name, name_length, hash,
                                       start, p);
                if (nxt_slow_path(nv == NULL)) {
                    return NULL;
                }
            }

            hash = NGX_HTTP_FIELD_HASH_INIT;
            valid = 1;
            name = NULL;
            start = p + 1;

        } else if (name == NULL) {
            hash = ngx_http_field_hash_char(hash, c);
        }
    }

    if (valid) {
        nv = ngx_http_argument(args, name, name_length, hash, start, p);
        if (nxt_slow_path(nv == NULL)) {
            return NULL;
        }
    }

    r->arguments = args;

    return args;
}


static ngx_http_name_value_t *
ngx_http_argument(nxt_array_t *array, u_char *name, size_t name_length,
    uint32_t hash, u_char *start, u_char *end)
{
    size_t                 length;
    ngx_http_name_value_t  *nv;

    nv = nxt_array_add(array);
    if (nxt_slow_path(nv == NULL)) {
        return NULL;
    }

    nv->hash = ngx_http_field_hash_end(hash) & 0xFFFF;

    length = end - start;

    if (name == NULL) {
        name_length = length;
        name = start;
        length = 0;
    }

    nv->name_length = name_length;
    nv->value_length = length;
    nv->name = name;
    nv->value = start;

    return nv;
}
