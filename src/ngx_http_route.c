
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#include <ngx_http_ctrl.h>


typedef enum {
    NGX_HTTP_ROUTE_TABLE = 0,
    NGX_HTTP_ROUTE_STRING,
    NGX_HTTP_ROUTE_STRING_PTR,
    NGX_HTTP_ROUTE_HOST,
    NGX_HTTP_ROUTE_HEADER,
    NGX_HTTP_ROUTE_ARGUMENT,
    NGX_HTTP_ROUTE_COOKIE,
    NGX_HTTP_ROUTE_SCHEME,
} ngx_http_route_object_t;


typedef enum {
    NGX_HTTP_ROUTE_PATTERN_EXACT = 0,
    NGX_HTTP_ROUTE_PATTERN_BEGIN,
    NGX_HTTP_ROUTE_PATTERN_MIDDLE,
    NGX_HTTP_ROUTE_PATTERN_END,
    NGX_HTTP_ROUTE_PATTERN_SUBSTRING,
} ngx_http_route_pattern_type_t;


typedef enum {
    NGX_HTTP_ROUTE_PATTERN_NOCASE = 0,
    NGX_HTTP_ROUTE_PATTERN_LOWCASE,
    NGX_HTTP_ROUTE_PATTERN_UPCASE,
} ngx_http_route_pattern_case_t;


typedef struct {
    u_char                         *start1;
    u_char                         *start2;
    uint32_t                       length1;
    uint32_t                       length2;
    uint32_t                       min_length;

    ngx_http_route_pattern_type_t  type:8;
    uint8_t                        case_sensitive;  /* 1 bit */
    uint8_t                        negative;        /* 1 bit */
    uint8_t                        any;             /* 1 bit */
} ngx_http_route_pattern_t;


typedef struct {
    /* The object must be the first field. */
    ngx_http_route_object_t        object:8;
    uint32_t                       items;

    union {
        uintptr_t                  offset;

        struct {
            u_char                 *start;
            uint16_t               hash;
            uint16_t               length;
        } name;
    } u;

    ngx_http_route_pattern_t       pattern[0];
} ngx_http_route_rule_t;


typedef struct {
    uint32_t                       items;
    ngx_http_route_rule_t          *rule[0];
} ngx_http_route_ruleset_t;


typedef struct {
    /* The object must be the first field. */
    ngx_http_route_object_t        object:8;
    uint32_t                       items;
    ngx_http_route_ruleset_t       *ruleset[0];
} ngx_http_route_table_t;


typedef union {
    ngx_http_route_rule_t          *rule;
    ngx_http_route_table_t         *table;
} ngx_http_route_test_t;


typedef struct {
    uint32_t                       items;
    ngx_http_action_t              action;
    ngx_http_route_test_t          test[0];
} ngx_http_route_match_t;


struct ngx_http_routes_s {
    uint32_t                       items;
    ngx_http_route_match_t         *match[0];
};


static ngx_http_route_match_t *ngx_http_route_match_create(ngx_http_conf_t *conf,
    nxt_conf_value_t *cv);
static ngx_http_route_table_t *ngx_http_route_table_create(ngx_http_conf_t *conf,
    nxt_conf_value_t *table_cv, ngx_http_route_object_t object,
    nxt_bool_t case_sensitive);
static ngx_http_route_ruleset_t *
    ngx_http_route_ruleset_create(ngx_http_conf_t *conf,
    nxt_conf_value_t *ruleset_cv, ngx_http_route_object_t object,
    nxt_bool_t case_sensitive);
static ngx_http_route_rule_t *
    ngx_http_route_rule_name_create(ngx_http_conf_t *conf,
    nxt_conf_value_t *rule_cv, nxt_str_t *name, nxt_bool_t case_sensitive);
static ngx_http_route_rule_t *ngx_http_route_rule_create(ngx_http_conf_t *conf,
    nxt_conf_value_t *cv, nxt_bool_t case_sensitive,
    ngx_http_route_pattern_case_t pattern_case);
static int nxt_http_pattern_compare(const void *one, const void *two);
static nxt_int_t ngx_http_route_pattern_create(nxt_mp_t *mp,
    nxt_conf_value_t *cv, ngx_http_route_pattern_t *pattern,
    ngx_http_route_pattern_case_t pattern_case);
static u_char *ngx_http_route_pattern_copy(nxt_mp_t *mp, nxt_str_t *test,
    ngx_http_route_pattern_case_t pattern_case);
static nxt_int_t ngx_http_route_action_create(ngx_http_conf_t *conf,
    nxt_conf_value_t *cv, ngx_http_route_match_t *match);

static ngx_http_action_t *ngx_http_route_match(ngx_http_request_t *r,
    ngx_http_route_match_t *match);
static nxt_int_t ngx_http_route_table(ngx_http_request_t *r,
    ngx_http_route_table_t *table);
static nxt_int_t ngx_http_route_ruleset(ngx_http_request_t *r,
    ngx_http_route_ruleset_t *ruleset);
static nxt_int_t ngx_http_route_rule(ngx_http_request_t *r,
    ngx_http_route_rule_t *rule);
static nxt_int_t ngx_http_route_headers(ngx_http_request_t *r,
    ngx_http_route_rule_t *rule);
static nxt_int_t ngx_http_route_arguments(ngx_http_request_t *r,
    ngx_http_route_rule_t *rule);
static nxt_int_t ngx_http_route_scheme(ngx_http_request_t *r,
    ngx_http_route_rule_t *rule);
static nxt_int_t ngx_http_route_host(ngx_http_request_t *r,
    ngx_http_route_rule_t *rule);
static nxt_int_t ngx_http_route_test_rule(ngx_http_route_rule_t *rule,
    u_char *start, size_t length);
static nxt_int_t ngx_http_route_pattern(ngx_http_route_pattern_t *pattern,
    u_char *start, size_t length);
static nxt_int_t ngx_http_route_memcmp(u_char *start, u_char *test,
    size_t length, nxt_bool_t case_sensitive);
static nxt_array_t *ngx_http_arguments_parse(ngx_http_request_t *r);


#define NGX_HTTP_FIELD_HASH_INIT        159406U
#define ngx_http_field_hash_char(h, c)  (((h) << 4) + (h) + (c))
#define ngx_http_field_hash_end(h)      (((h) >> 16) ^ (h))


ngx_http_routes_t *
ngx_http_routes_create(ngx_http_conf_t *conf, nxt_conf_value_t *routes_conf)
{
    size_t                  size;
    uint32_t                i, n;
    nxt_conf_value_t        *value;
    ngx_http_routes_t       *routes;
    ngx_http_route_match_t  *match, **m;

    n = nxt_conf_array_elements_count(routes_conf);
    size = sizeof(ngx_http_routes_t) + n * sizeof(ngx_http_route_match_t *);

    routes = nxt_mp_alloc(conf->pool, size);
    if (nxt_slow_path(routes == NULL)) {
        return NULL;
    }

    routes->items = n;
    m = &routes->match[0];

    for (i = 0; i < n; i++) {
        value = nxt_conf_get_array_element(routes_conf, i);

        match = ngx_http_route_match_create(conf, value);
        if (match == NULL) {
            return NULL;
        }

        *m++ = match;
    }

    return routes;
}


typedef struct {
    nxt_conf_value_t               *host;
    nxt_conf_value_t               *uri;
    nxt_conf_value_t               *method;
    nxt_conf_value_t               *headers;
    nxt_conf_value_t               *arguments;
    nxt_conf_value_t               *scheme;
} ngx_http_route_match_conf_t;


static nxt_conf_map_t  ngx_http_route_match_conf[] = {
    {
        nxt_string("scheme"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_match_conf_t, scheme)
    },
    {
        nxt_string("host"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_match_conf_t, host),
    },

    {
        nxt_string("uri"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_match_conf_t, uri),
    },

    {
        nxt_string("method"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_match_conf_t, method),
    },

    {
        nxt_string("headers"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_match_conf_t, headers),
    },

    {
        nxt_string("arguments"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_match_conf_t, arguments),
    },
};


static ngx_http_route_match_t *
ngx_http_route_match_create(ngx_http_conf_t *conf, nxt_conf_value_t *cv)
{
    size_t                       size;
    uint32_t                     n;
    nxt_int_t                    ret;
    nxt_conf_value_t             *match_conf;
    ngx_http_route_test_t        *test;
    ngx_http_route_rule_t        *rule;
    ngx_http_route_table_t       *table;
    ngx_http_route_match_t       *match;
    ngx_http_route_match_conf_t  mtcf;

    static nxt_str_t  match_path = nxt_string("/match");

    match_conf = nxt_conf_get_path(cv, &match_path);

    n = (match_conf != NULL) ? nxt_conf_object_members_count(match_conf) : 0;
    size = sizeof(ngx_http_route_match_t) + n * sizeof(ngx_http_route_rule_t *);

    match = nxt_mp_zalloc(conf->pool, size);
    if (nxt_slow_path(match == NULL)) {
        return NULL;
    }

    match->items = n;

    if (n == 0) {
        goto action_create;
    }

    nxt_memzero(&mtcf, sizeof(mtcf));

    ret = nxt_conf_map_object(conf->pool, match_conf, ngx_http_route_match_conf,
                              nxt_nitems(ngx_http_route_match_conf), &mtcf);
    if (ret != NXT_OK) {
        return NULL;
    }

    test = &match->test[0];

    if (mtcf.scheme != NULL) {
        rule = ngx_http_route_rule_create(conf, mtcf.scheme, 1,
                                          NGX_HTTP_ROUTE_PATTERN_NOCASE);
        if (rule == NULL) {
            return NULL;
        }

        rule->object = NGX_HTTP_ROUTE_SCHEME;
        test->rule = rule;
        test++;
    }

    if (mtcf.uri != NULL) {
        rule = ngx_http_route_rule_create(conf, mtcf.uri, 1,
                                          NGX_HTTP_ROUTE_PATTERN_NOCASE);
        if (rule == NULL) {
            return NULL;
        }

        rule->u.offset = offsetof(ngx_http_request_t, uri);
        rule->object = NGX_HTTP_ROUTE_STRING;
        test->rule = rule;
        test++;
    }

    if (mtcf.method != NULL) {
        rule = ngx_http_route_rule_create(conf, mtcf.method, 1,
                                          NGX_HTTP_ROUTE_PATTERN_UPCASE);
        if (rule == NULL) {
            return NULL;
        }

        rule->u.offset = offsetof(ngx_http_request_t, method_name);
        rule->object = NGX_HTTP_ROUTE_STRING;
        test->rule = rule;
        test++;
    }

    if (mtcf.host != NULL) {
        rule = ngx_http_route_rule_create(conf, mtcf.host, 1,
                                          NGX_HTTP_ROUTE_PATTERN_LOWCASE);
        if (rule == NULL) {
            return NULL;
        }

        rule->object = NGX_HTTP_ROUTE_HOST;
        test->rule = rule;
        test++;
    }

    if (mtcf.headers != NULL) {
        table = ngx_http_route_table_create(conf, mtcf.headers,
                                            NGX_HTTP_ROUTE_HEADER, 0);
        if (table == NULL) {
            return NULL;
        }

        test->table = table;
        test++;
    }

    if (mtcf.arguments != NULL) {
        table = ngx_http_route_table_create(conf, mtcf.arguments,
                                            NGX_HTTP_ROUTE_ARGUMENT, 1);
        if (table == NULL) {
            return NULL;
        }

        test->table = table;
        test++;
    }

action_create:

    ret = ngx_http_route_action_create(conf, cv, match);
    if (nxt_slow_path(ret != NXT_OK)) {
        return NULL;
    }

    return match;
}


static ngx_http_route_table_t *
ngx_http_route_table_create(ngx_http_conf_t *conf, nxt_conf_value_t *table_cv,
    ngx_http_route_object_t object, nxt_bool_t case_sensitive)
{
    size_t                    size;
    uint32_t                  i, n;
    nxt_bool_t                array;
    nxt_conf_value_t          *ruleset_cv;
    ngx_http_route_table_t    *table;
    ngx_http_route_ruleset_t  *ruleset;

    array = (nxt_conf_type(table_cv) == NXT_CONF_ARRAY);
    n = array ? nxt_conf_array_elements_count(table_cv) : 1;
    size = sizeof(ngx_http_route_table_t)
           + n * sizeof(ngx_http_route_ruleset_t *);

    table = nxt_mp_alloc(conf->pool, size);
    if (nxt_slow_path(table == NULL)) {
        return NULL;
    }

    table->items = n;
    table->object = NGX_HTTP_ROUTE_TABLE;

    if (!array) {
        ruleset = ngx_http_route_ruleset_create(conf, table_cv, object,
                                                case_sensitive);
        if (nxt_slow_path(ruleset == NULL)) {
            return NULL;
        }

        table->ruleset[0] = ruleset;

        return table;
    }

    for (i = 0; i < n; i++) {
        ruleset_cv = nxt_conf_get_array_element(table_cv, i);

        ruleset = ngx_http_route_ruleset_create(conf, ruleset_cv, object,
                                                case_sensitive);
        if (nxt_slow_path(ruleset == NULL)) {
            return NULL;
        }

        table->ruleset[i] = ruleset;
    }

    return table;
}


static ngx_http_route_ruleset_t *
ngx_http_route_ruleset_create(ngx_http_conf_t *conf, nxt_conf_value_t *ruleset_cv,
    ngx_http_route_object_t object, nxt_bool_t case_sensitive)
{
    size_t                    size;
    uint32_t                  i, n, next;
    nxt_str_t                 name;
    nxt_conf_value_t          *rule_cv;
    ngx_http_route_rule_t     *rule;
    ngx_http_route_ruleset_t  *ruleset;

    n = nxt_conf_object_members_count(ruleset_cv);
    size = sizeof(ngx_http_route_ruleset_t)
           + n * sizeof(ngx_http_route_rule_t *);

    ruleset = nxt_mp_alloc(conf->pool, size);
    if (nxt_slow_path(ruleset == NULL)) {
        return NULL;
    }

    ruleset->items = n;

    next = 0;

    for (i = 0; i < n; i++) {
        rule_cv = nxt_conf_next_object_member(ruleset_cv, &name, &next);

        rule = ngx_http_route_rule_name_create(conf, rule_cv, &name,
                                               case_sensitive);
        if (nxt_slow_path(rule == NULL)) {
            return NULL;
        }

        rule->object = object;
        ruleset->rule[i] = rule;
    }

    return ruleset;
}


static ngx_http_route_rule_t *
ngx_http_route_rule_name_create(ngx_http_conf_t *conf, nxt_conf_value_t *rule_cv,
    nxt_str_t *name, nxt_bool_t case_sensitive)
{
    u_char                 c, *p;
    uint32_t               hash;
    nxt_uint_t             i;
    ngx_http_route_rule_t  *rule;

    rule = ngx_http_route_rule_create(conf, rule_cv, case_sensitive,
                                      NGX_HTTP_ROUTE_PATTERN_NOCASE);
    if (nxt_slow_path(rule == NULL)) {
        return NULL;
    }

    rule->u.name.length = name->length;

    p = nxt_mp_nget(conf->pool, name->length);
    if (nxt_slow_path(p == NULL)) {
        return NULL;
    }

    rule->u.name.start = p;

    hash = NGX_HTTP_FIELD_HASH_INIT;

    for (i = 0; i < name->length; i++) {
        c = name->start[i];
        *p++ = c;

        c = case_sensitive ? c : nxt_lowcase(c);
        hash = ngx_http_field_hash_char(hash, c);
    }

    rule->u.name.hash = ngx_http_field_hash_end(hash) & 0xFFFF;

    return rule;
}


static ngx_http_route_rule_t *
ngx_http_route_rule_create(ngx_http_conf_t *conf, nxt_conf_value_t *cv,
    nxt_bool_t case_sensitive, ngx_http_route_pattern_case_t pattern_case)
{
    size_t                    size;
    uint32_t                  i, n;
    nxt_int_t                 ret;
    nxt_bool_t                string;
    nxt_conf_value_t          *value;
    ngx_http_route_rule_t     *rule;
    ngx_http_route_pattern_t  *pattern;

    string = (nxt_conf_type(cv) != NXT_CONF_ARRAY);
    n = string ? 1 : nxt_conf_array_elements_count(cv);
    size = sizeof(ngx_http_route_rule_t) + n * sizeof(ngx_http_route_pattern_t);

    rule = nxt_mp_alloc(conf->pool, size);
    if (nxt_slow_path(rule == NULL)) {
        return NULL;
    }

    rule->items = n;

    pattern = &rule->pattern[0];

    if (string) {
        pattern[0].case_sensitive = case_sensitive;
        ret = ngx_http_route_pattern_create(conf->pool, cv, &pattern[0],
                                            pattern_case);
        if (nxt_slow_path(ret != NXT_OK)) {
            return NULL;
        }

        return rule;
    }

    nxt_conf_array_qsort(cv, nxt_http_pattern_compare);

    for (i = 0; i < n; i++) {
        pattern[i].case_sensitive = case_sensitive;
        value = nxt_conf_get_array_element(cv, i);

        ret = ngx_http_route_pattern_create(conf->pool, value, &pattern[i],
                                            pattern_case);
        if (nxt_slow_path(ret != NXT_OK)) {
            return NULL;
        }
    }

    return rule;
}


static int
nxt_http_pattern_compare(const void *one, const void *two)
{
    nxt_str_t         test;
    nxt_bool_t        negative1, negative2;
    nxt_conf_value_t  *value;

    value = (nxt_conf_value_t *) one;
    nxt_conf_get_string(value, &test);
    negative1 = (test.length != 0 && test.start[0] == '!');

    value = (nxt_conf_value_t *) two;
    nxt_conf_get_string(value, &test);
    negative2 = (test.length != 0 && test.start[0] == '!');

    return (negative2 - negative1);
}


static nxt_int_t
ngx_http_route_pattern_create(nxt_mp_t *mp, nxt_conf_value_t *cv,
    ngx_http_route_pattern_t *pattern, ngx_http_route_pattern_case_t pattern_case)
{
    u_char                         *start;
    nxt_str_t                      test;
    nxt_uint_t                     n, length;
    ngx_http_route_pattern_type_t  type;

    /* Suppress warning about uninitialized variable. */
    length = 0;

    type = NGX_HTTP_ROUTE_PATTERN_EXACT;

    nxt_conf_get_string(cv, &test);

    pattern->negative = 0;
    pattern->any = 1;

    if (test.length != 0) {

        if (test.start[0] == '!') {
            test.start++;
            test.length--;

            pattern->negative = 1;
            pattern->any = 0;
        }

        if (test.length != 0) {

            if (test.start[0] == '*') {
                test.start++;
                test.length--;

                if (test.length != 0) {
                    if (test.start[test.length - 1] == '*') {
                        test.length--;
                        type = NGX_HTTP_ROUTE_PATTERN_SUBSTRING;

                    } else {
                        type = NGX_HTTP_ROUTE_PATTERN_END;
                    }

                } else {
                    type = NGX_HTTP_ROUTE_PATTERN_BEGIN;
                }

            } else if (test.start[test.length - 1] == '*') {
                test.length--;
                type = NGX_HTTP_ROUTE_PATTERN_BEGIN;

            } else {
                length = test.length - 1;

                for (n = 1; n < length; n++) {
                    if (test.start[n] == '*') {
                        test.length = n;
                        type = NGX_HTTP_ROUTE_PATTERN_MIDDLE;
                        break;
                    }
                }
            }
        }
    }

    pattern->type = type;
    pattern->min_length = test.length;
    pattern->length1 = test.length;

    start = ngx_http_route_pattern_copy(mp, &test, pattern_case);
    if (nxt_slow_path(start == NULL)) {
        return NXT_ERROR;
    }

    pattern->start1 = start;

    if (type == NGX_HTTP_ROUTE_PATTERN_MIDDLE) {
        length -= test.length;
        pattern->length2 = length;
        pattern->min_length += length;

        test.start = &test.start[test.length + 1];
        test.length = length;

        start = ngx_http_route_pattern_copy(mp, &test, pattern_case);
        if (nxt_slow_path(start == NULL)) {
            return NXT_ERROR;
        }

        pattern->start2 = start;
    }

    return NXT_OK;
}


static u_char *
ngx_http_route_pattern_copy(nxt_mp_t *mp, nxt_str_t *test,
    ngx_http_route_pattern_case_t pattern_case)
{
    u_char  *start;

    start = nxt_mp_nget(mp, test->length);
    if (nxt_slow_path(start == NULL)) {
        return start;
    }

    switch (pattern_case) {

    case NGX_HTTP_ROUTE_PATTERN_UPCASE:
        nxt_memcpy_upcase(start, test->start, test->length);
        break;

    case NGX_HTTP_ROUTE_PATTERN_LOWCASE:
        nxt_memcpy_lowcase(start, test->start, test->length);
        break;

    case NGX_HTTP_ROUTE_PATTERN_NOCASE:
        nxt_memcpy(start, test->start, test->length);
        break;
    }

    return start;
}


typedef struct {
    nxt_conf_value_t               *variables;
    nxt_conf_value_t               *blacklist;
    nxt_conf_value_t               *whitelist;
    nxt_conf_value_t               *add_headers;
    nxt_conf_value_t               *limit_conn;
    nxt_conf_value_t               *limit_req;
    nxt_uint_t                     limit_rate;
    nxt_uint_t                     return_status;
    nxt_str_t                      return_text;
    nxt_str_t                      return_location;
} ngx_http_route_action_conf_t;


static nxt_conf_map_t  ngx_http_route_action_conf[] = {

    {
        nxt_string("variables"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_action_conf_t, variables)
    },

    {
        nxt_string("blacklist"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_action_conf_t, blacklist),
    },

    {
        nxt_string("whitelist"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_action_conf_t, whitelist),
    },

    {
        nxt_string("add_headers"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_action_conf_t, add_headers)
    },

    {
        nxt_string("limit_conn"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_action_conf_t, limit_conn)
    },

    {
        nxt_string("limit_req"),
        NXT_CONF_MAP_PTR,
        offsetof(ngx_http_route_action_conf_t, limit_req)
    },

    {
        nxt_string("limit_rate"),
        NXT_CONF_MAP_INT32,
        offsetof(ngx_http_route_action_conf_t, limit_rate)
    },

    {
        nxt_string("return"),
        NXT_CONF_MAP_INT32,
        offsetof(ngx_http_route_action_conf_t, return_status),
    },

    {
        nxt_string("text"),
        NXT_CONF_MAP_STR,
        offsetof(ngx_http_route_action_conf_t, return_text),
    },

    {
        nxt_string("location"),
        NXT_CONF_MAP_STR,
        offsetof(ngx_http_route_action_conf_t, return_location),
    },
};


static nxt_conf_map_t  ngx_http_route_action_limit_conn_conf[] = {
    {
        nxt_string("key"),
        NXT_CONF_MAP_STR,
        offsetof(ngx_http_action_limit_conn_t, key),
    },

    {
        nxt_string("conn"),
        NXT_CONF_MAP_INT32,
        offsetof(ngx_http_action_limit_conn_t, conn),
    },
};


static nxt_conf_map_t  ngx_http_route_action_limit_req_conf[] = {
    {
        nxt_string("key"),
        NXT_CONF_MAP_STR,
        offsetof(ngx_http_action_limit_req_t, key),
    },

    {
        nxt_string("rate"),
        NXT_CONF_MAP_INT32,
        offsetof(ngx_http_action_limit_req_t, rate),
    },

    {
        nxt_string("burst"),
        NXT_CONF_MAP_INT32,
        offsetof(ngx_http_action_limit_req_t, burst),
    },
};


static nxt_int_t
ngx_http_route_action_create(ngx_http_conf_t *conf, nxt_conf_value_t *cv,
    ngx_http_route_match_t *match)
{
    size_t                         size;
    uint32_t                       i, n, next;
    nxt_mp_t                       *mp;
    nxt_int_t                      ret;
    nxt_str_t                      name, value;
    nxt_bool_t                     array;
    nxt_conf_value_t               *action_conf, *addr_conf;
    nxt_conf_value_t               *headers_conf, *header_conf;
    nxt_conf_value_t               *variables_conf, *variable_conf;
    nxt_conf_value_t               *blacklist_conf, *whitelist_conf;
    nxt_conf_value_t               *limit_conn_conf, *limit_req_conf;
    nxt_addr_pattern_t             *pattern;
    ngx_http_name_value_t          *nv;
    ngx_http_action_addr_t         *blacklist, *whitelist;
    ngx_http_action_headers_t      *headers;
    ngx_http_action_variables_t    *variables;
    ngx_http_action_limit_req_t    *limit_req;
    ngx_http_action_limit_conn_t   *limit_conn;
    ngx_http_route_action_conf_t   accf;

    static nxt_str_t  action_path = nxt_string("/action");

    mp = conf->pool;

    action_conf = nxt_conf_get_path(cv, &action_path);
    if (action_conf == NULL) {
        return NXT_ERROR;
    }

    nxt_memzero(&accf, sizeof(accf));

    ret = nxt_conf_map_object(mp, action_conf, ngx_http_route_action_conf,
                              nxt_nitems(ngx_http_route_action_conf), &accf);
    if (ret != NXT_OK) {
        return ret;
    }

    variables_conf = accf.variables;

    if (variables_conf != NULL) {

        n = nxt_conf_object_members_count(variables_conf);

        size = sizeof(ngx_http_action_variables_t)
               + n * sizeof(ngx_http_name_value_t *);

        variables = nxt_mp_alloc(mp, size);
        if (nxt_slow_path(variables == NULL)) {
            return NXT_ERROR;
        }

        variables->items = n;

        next = 0;

        for (i = 0; i < n; i++) {
            nv = nxt_mp_alloc(mp, sizeof(ngx_http_name_value_t));
            if (nxt_slow_path(nv == NULL)) {
                return NXT_ERROR;
            }

            variable_conf = nxt_conf_next_object_member(variables_conf, &name, &next);
            nxt_conf_get_string(variable_conf, &value);

            nv->name_length = name.length;
            nv->name = name.start;
            nv->value_length = value.length;
            nv->value = value.start;

            variables->variable[i] = nv;
        }

        match->action.variables = variables;
    }

    blacklist_conf = accf.blacklist;

    if (blacklist_conf != NULL) {

        array = (nxt_conf_type(blacklist_conf) == NXT_CONF_ARRAY);
        n = array ? nxt_conf_array_elements_count(blacklist_conf) : 1;

        size = sizeof(ngx_http_action_addr_t)
               + n * sizeof(nxt_addr_pattern_t *);

        blacklist = nxt_mp_alloc(mp, size);
        if (nxt_slow_path(blacklist == NULL)) {
            return NXT_ERROR;
        }

        blacklist->items = n;

        if (!array) {
            pattern = &blacklist->addr_pattern[0];

            ret = nxt_addr_pattern_parse(mp, pattern, blacklist_conf);
            if (ret != NXT_OK) {
                return ret;
            }

        } else {
            for (i = 0; i < n; i++) {
                pattern = &blacklist->addr_pattern[i];
                addr_conf = nxt_conf_get_array_element(blacklist_conf, i);

                ret = nxt_addr_pattern_parse(mp, pattern, addr_conf);
                if (ret != NXT_OK) {
                    return ret;
                }
            }
        }

        match->action.blacklist = blacklist;
    }

    whitelist_conf = accf.whitelist;

    if (whitelist_conf != NULL) {

        array = (nxt_conf_type(whitelist_conf) == NXT_CONF_ARRAY);
        n = array ? nxt_conf_array_elements_count(whitelist_conf) : 1;

        size = sizeof(ngx_http_action_addr_t)
               + n * sizeof(nxt_addr_pattern_t *);

        whitelist = nxt_mp_alloc(mp, size);
        if (nxt_slow_path(whitelist == NULL)) {
            return NXT_ERROR;
        }

        whitelist->items = n;

        if (!array) {
            pattern = &whitelist->addr_pattern[0];

            ret = nxt_addr_pattern_parse(mp, pattern, whitelist_conf);
            if (ret != NXT_OK) {
                return ret;
            }

        } else {
            for (i = 0; i < n; i++) {
                pattern = &whitelist->addr_pattern[i];
                addr_conf = nxt_conf_get_array_element(whitelist_conf, i);

                ret = nxt_addr_pattern_parse(mp, pattern, addr_conf);
                if (ret != NXT_OK) {
                    return ret;
                }
            }
        }

        match->action.whitelist = whitelist;
    }

    headers_conf = accf.add_headers;

    if (headers_conf != NULL) {

        n = nxt_conf_object_members_count(headers_conf);

        size = sizeof(ngx_http_action_headers_t)
               + n * sizeof(ngx_http_name_value_t *);

        headers = nxt_mp_alloc(mp, size);
        if (nxt_slow_path(headers == NULL)) {
            return NXT_ERROR;
        }

        headers->items = n;

        next = 0;

        for (i = 0; i < n; i++) {
            nv = nxt_mp_alloc(mp, sizeof(ngx_http_name_value_t));
            if (nxt_slow_path(nv == NULL)) {
                return NXT_ERROR;
            }

            header_conf = nxt_conf_next_object_member(headers_conf, &name, &next);
            nxt_conf_get_string(header_conf, &value);

            nv->name_length = name.length;
            nv->name = name.start;
            nv->value_length = value.length;
            nv->value = value.start;

            headers->header[i] = nv;
        }

        match->action.add_headers = headers;
    }

    limit_conn_conf = accf.limit_conn;

    if (limit_conn_conf != NULL) {
        limit_conn = nxt_mp_alloc(mp, sizeof(ngx_http_action_limit_conn_t));
        if (nxt_slow_path(limit_conn == NULL)) {
            return NXT_ERROR;
        }

        ret = nxt_conf_map_object(mp, limit_conn_conf,
                                  ngx_http_route_action_limit_conn_conf,
                                  nxt_nitems(ngx_http_route_action_limit_conn_conf),
                                  limit_conn);
        if (ret != NXT_OK) {
            return ret;
        }

        match->action.limit_conn = limit_conn;
    }

    limit_req_conf = accf.limit_req;

    if (limit_req_conf != NULL) {
        limit_req = nxt_mp_alloc(mp, sizeof(ngx_http_action_limit_req_t));
        if (nxt_slow_path(limit_req == NULL)) {
            return NXT_ERROR;
        }

        ret = nxt_conf_map_object(mp, limit_req_conf,
                                  ngx_http_route_action_limit_req_conf,
                                  nxt_nitems(ngx_http_route_action_limit_req_conf),
                                  limit_req);
        if (ret != NXT_OK) {
            return ret;
        }

        limit_req->rate *= 1000;

        match->action.limit_req = limit_req;
    }

    match->action.limit_rate = accf.limit_rate;

    match->action.return_status = accf.return_status;
    match->action.return_text = accf.return_text;
    match->action.return_location = accf.return_location;

    return NXT_OK;
}


ngx_http_action_t *
ngx_http_route_action(ngx_http_request_t *r, ngx_http_routes_t *routes)
{
    ngx_http_action_t       *action;
    ngx_http_route_match_t  **match, **end;

    match = &routes->match[0];
    end = match + routes->items;

    while (match < end) {
        action = ngx_http_route_match(r, *match);
        if (action != NULL) {
            return action;
        }

        match++;
    }

    return NULL;
}


static ngx_http_action_t *
ngx_http_route_match(ngx_http_request_t *r, ngx_http_route_match_t *match)
{
    nxt_int_t              ret;
    ngx_http_route_test_t  *test, *end;

    test = &match->test[0];
    end = test + match->items;

    while (test < end) {

        switch (test->rule->object) {

        case NGX_HTTP_ROUTE_TABLE:
            ret = ngx_http_route_table(r, test->table);
            break;

        default:
            ret = ngx_http_route_rule(r, test->rule);
            break;
        }

        if (ret <= 0) {
            /* 0 => NULL, -1 => NGX_HTTP_ACTION_ERROR. */
            return (ngx_http_action_t *) (intptr_t) ret;
        }

        test++;
    }

    return &match->action;
}


static nxt_int_t
ngx_http_route_table(ngx_http_request_t *r, ngx_http_route_table_t *table)
{
    nxt_int_t                 ret;
    ngx_http_route_ruleset_t  **ruleset, **end;

    ret = 1;
    ruleset = &table->ruleset[0];
    end = ruleset + table->items;

    while (ruleset < end) {
        ret = ngx_http_route_ruleset(r, *ruleset);

        if (ret != 0) {
            return ret;
        }

        ruleset++;
    }

    return ret;
}


static nxt_int_t
ngx_http_route_ruleset(ngx_http_request_t *r, ngx_http_route_ruleset_t *ruleset)
{
    nxt_int_t              ret;
    ngx_http_route_rule_t  **rule, **end;

    rule = &ruleset->rule[0];
    end = rule + ruleset->items;

    while (rule < end) {
        ret = ngx_http_route_rule(r, *rule);

        if (ret <= 0) {
            return ret;
        }

        rule++;
    }

    return 1;
}


static nxt_int_t
ngx_http_route_rule(ngx_http_request_t *r, ngx_http_route_rule_t *rule)
{
    void                *p, **pp;
    u_char              *start;
    size_t               length;
    nxt_str_t           *s;

    switch (rule->object) {

    case NGX_HTTP_ROUTE_HEADER:
        return ngx_http_route_headers(r, rule);

    case NGX_HTTP_ROUTE_ARGUMENT:
        return ngx_http_route_arguments(r, rule);

    case NGX_HTTP_ROUTE_SCHEME:
        return ngx_http_route_scheme(r, rule);

    case NGX_HTTP_ROUTE_HOST:
        return ngx_http_route_host(r, rule);

    default:
        break;
    }

    p = nxt_pointer_to(r, rule->u.offset);

    if (rule->object == NGX_HTTP_ROUTE_STRING) {
        s = p;

    } else {
        /* NGX_HTTP_ROUTE_STRING_PTR */
        pp = p;
        s = *pp;

        if (s == NULL) {
            return 0;
        }
    }

    length = s->length;
    start = s->start;

    return ngx_http_route_test_rule(rule, start, length);
}


static nxt_int_t
ngx_http_route_headers(ngx_http_request_t *r, ngx_http_route_rule_t *rule)
{
    u_char               c;
    uint32_t             hash;
    nxt_int_t            ret;
    ngx_str_t           *name, *value;
    ngx_uint_t           i, j;
    ngx_list_part_t     *part;
    ngx_table_elt_t     *header;

    ret = 0;
        
    part = &r->headers_in.headers.part;
    header = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        name = &header[i].key;
        value = &header[i].value;

        hash = NGX_HTTP_FIELD_HASH_INIT;

        for (j = 0; j < name->len; j++) {
            c = name->data[j];
            c = nxt_lowcase(c);
            hash = ngx_http_field_hash_char(hash, c);
        }

        hash = ngx_http_field_hash_end(hash) & 0xFFFF;

        if (rule->u.name.hash != hash
            || rule->u.name.length != name->len
            || nxt_strncasecmp(rule->u.name.start, name->data, name->len)
               != 0)
        {
            continue;
        }

        ret = ngx_http_route_test_rule(rule, value->data, value->len);

        if (ret == 0) {
            return ret;
        }
    }

    return ret;
}


static nxt_int_t
ngx_http_route_arguments(ngx_http_request_t *r, ngx_http_route_rule_t *rule)
{
    nxt_bool_t             ret;
    nxt_array_t            *arguments;
    ngx_http_name_value_t  *nv, *end;

    if (r->args.data == NULL) {
        return 0;
    }

    arguments = ngx_http_arguments_parse(r);
    if (nxt_slow_path(arguments == NULL)) {
        return -1;
    }

    ret = 0;

    nv = arguments->elts;
    end = nv + arguments->nelts;

    while (nv < end) {

        if (rule->u.name.hash == nv->hash
            && rule->u.name.length == nv->name_length
            && nxt_memcmp(rule->u.name.start, nv->name, nv->name_length) == 0)
        {
            ret = ngx_http_route_test_rule(rule, nv->value, nv->value_length);
            if (ret == 0) {
                break;
            }
        }

        nv++;
    }

    return ret;
}


static nxt_int_t
ngx_http_route_scheme(ngx_http_request_t *r, ngx_http_route_rule_t *rule)
{
    void        *ssl;
    nxt_bool_t  tls, https;

    ssl = NULL;

#if (NGX_HTTP_SSL)
    if (r->connection->ssl) {
        ssl = r->connection->ssl;
    }
#endif

    tls = (ssl != NULL);

    https = (rule->pattern[0].length1 == nxt_length("https"));

    return (tls == https);
}


static nxt_int_t
ngx_http_route_host(ngx_http_request_t *r, ngx_http_route_rule_t *rule)
{
    u_char     *start;
    size_t      length;

    length = r->headers_in.server.len;
    start = r->headers_in.server.data;

    return ngx_http_route_test_rule(rule, start, length);
}


static nxt_int_t
ngx_http_route_test_rule(ngx_http_route_rule_t *rule, u_char *start,
    size_t length)
{
    nxt_int_t                 ret;
    ngx_http_route_pattern_t  *pattern, *end;

    ret = 1;
    pattern = &rule->pattern[0];
    end = pattern + rule->items;

    while (pattern < end) {
        ret = ngx_http_route_pattern(pattern, start, length);

        /* ngx_http_route_pattern() returns either 1 or 0. */
        ret ^= pattern->negative;

        if (pattern->any == ret) {
            return ret;
        }

        pattern++;
    }

    return ret;
}


static nxt_int_t
ngx_http_route_pattern(ngx_http_route_pattern_t *pattern, u_char *start,
    size_t length)
{
    u_char     *p, *end, *test;
    size_t     test_length;
    nxt_int_t  ret;

    if (length < pattern->min_length) {
        return 0;
    }

    test = pattern->start1;
    test_length = pattern->length1;

    switch (pattern->type) {

    case NGX_HTTP_ROUTE_PATTERN_EXACT:
        if (length != test_length) {
            return 0;
        }

        break;

    case NGX_HTTP_ROUTE_PATTERN_BEGIN:
        break;

    case NGX_HTTP_ROUTE_PATTERN_MIDDLE:
        ret = ngx_http_route_memcmp(start, test, test_length,
                                    pattern->case_sensitive);
        if (!ret) {
            return ret;
        }

        test = pattern->start2;
        test_length = pattern->length2;

        /* Fall through. */
    
    case NGX_HTTP_ROUTE_PATTERN_END:
        start += length - test_length;
        break;

    case NGX_HTTP_ROUTE_PATTERN_SUBSTRING:
        end = start + length;

        if (pattern->case_sensitive) {
            p = nxt_memstrn(start, end, (char *) test, test_length);

        } else {
            p = nxt_memcasestrn(start, end, (char *) test, test_length);
        }

        return (p != NULL);
    }

    return ngx_http_route_memcmp(start, test, test_length,
                                 pattern->case_sensitive);
}


static nxt_int_t
ngx_http_route_memcmp(u_char *start, u_char *test, size_t test_length,
    nxt_bool_t case_sensitive)
{
    nxt_int_t  n;

    if (case_sensitive) {
        n = nxt_memcmp(start, test, test_length);

    } else {
        n = nxt_memcasecmp(start, test, test_length);
    }

    return (n == 0);
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


static nxt_array_t *
ngx_http_arguments_parse(ngx_http_request_t *r)
{
    size_t                 name_length;
    u_char                 c, *p, *start, *end, *name;
    uint32_t               hash;
    nxt_bool_t             valid;
    nxt_array_t            *args;
    ngx_http_ctrl_ctx_t    *ctx;
    ngx_http_name_value_t  *nv;

    ctx = ngx_http_get_module_ctx(r, ngx_http_ctrl_module);

    args = nxt_array_create(ctx->mem_pool, 2, sizeof(ngx_http_name_value_t));
    if (nxt_slow_path(args == NULL)) {
        return NULL;
    }

    hash = NGX_HTTP_FIELD_HASH_INIT;
    valid = 1;
    name = NULL;
    name_length = 0;

    start = r->args.data;
    end = start + r->args.len;

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

    return args;
}
