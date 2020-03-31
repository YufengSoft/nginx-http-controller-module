
/*
 * Copyright (C) Valentin V. Bartenev
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>


typedef enum {
    NXT_CONF_VLDT_NULL    = 1 << NXT_CONF_NULL,
    NXT_CONF_VLDT_BOOLEAN = 1 << NXT_CONF_BOOLEAN,
    NXT_CONF_VLDT_INTEGER = 1 << NXT_CONF_INTEGER,
    NXT_CONF_VLDT_NUMBER  = (1 << NXT_CONF_NUMBER) | NXT_CONF_VLDT_INTEGER,
    NXT_CONF_VLDT_STRING  = 1 << NXT_CONF_STRING,
    NXT_CONF_VLDT_ARRAY   = 1 << NXT_CONF_ARRAY,
    NXT_CONF_VLDT_OBJECT  = 1 << NXT_CONF_OBJECT,
} nxt_conf_vldt_type_t;


typedef struct {
    nxt_str_t             name;
    nxt_conf_vldt_type_t  type;
    nxt_int_t             (*validator)(nxt_conf_validation_t *vldt,
                                       nxt_conf_value_t *value, void *data);
    void                  *data;
} nxt_conf_vldt_object_t;


#define NXT_CONF_VLDT_NEXT(f)  { nxt_null_string, 0, NULL, (f) }
#define NXT_CONF_VLDT_END      { nxt_null_string, 0, NULL, NULL }


typedef nxt_int_t (*nxt_conf_vldt_member_t)(nxt_conf_validation_t *vldt,
                                            nxt_str_t *name,
                                            nxt_conf_value_t *value);
typedef nxt_int_t (*nxt_conf_vldt_element_t)(nxt_conf_validation_t *vldt,
                                             nxt_conf_value_t *value);

static nxt_int_t nxt_conf_vldt_type(nxt_conf_validation_t *vldt,
    nxt_str_t *name, nxt_conf_value_t *value, nxt_conf_vldt_type_t type);
static nxt_int_t nxt_conf_vldt_error(nxt_conf_validation_t *vldt,
    const char *fmt, ...);

static nxt_int_t nxt_conf_vldt_object(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);
static nxt_int_t nxt_conf_vldt_object_iterator(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);
static nxt_int_t nxt_conf_vldt_array_iterator(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);

static nxt_int_t nxt_conf_vldt_action(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);

static nxt_int_t nxt_conf_vldt_route(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value);
static nxt_int_t nxt_conf_vldt_match_patterns(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);
static nxt_int_t nxt_conf_vldt_match_pattern(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value);
static nxt_int_t nxt_conf_vldt_match_patterns_sets(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);
static nxt_int_t nxt_conf_vldt_match_patterns_set(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value);
static nxt_int_t nxt_conf_vldt_match_patterns_set_member(
    nxt_conf_validation_t *vldt, nxt_str_t *name, nxt_conf_value_t *value);
static nxt_int_t nxt_conf_vldt_match_scheme_pattern(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);
static nxt_int_t nxt_conf_vldt_match_addrs(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);
static nxt_int_t nxt_conf_vldt_match_addr(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value);

static nxt_int_t nxt_conf_vldt_upstream(nxt_conf_validation_t *vldt,
     nxt_str_t *name, nxt_conf_value_t *value);
static nxt_int_t nxt_conf_vldt_server(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value);
static nxt_int_t nxt_conf_vldt_server_address(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);
static nxt_int_t nxt_conf_vldt_server_weight(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);
static nxt_int_t nxt_conf_vldt_server_max_conns(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);
static nxt_int_t nxt_conf_vldt_server_max_fails(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);
static nxt_int_t nxt_conf_vldt_server_fail_timeout(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data);

static nxt_int_t nxt_conf_vldt_variable(nxt_conf_validation_t *vldt,
    nxt_str_t *name, nxt_conf_value_t *value);
static nxt_int_t nxt_conf_vldt_add_header(nxt_conf_validation_t *vldt,
    nxt_str_t *name, nxt_conf_value_t *value);


static nxt_conf_vldt_object_t  nxt_conf_vldt_root_members[] = {

    { nxt_string("routes"),
      NXT_CONF_VLDT_ARRAY,
      &nxt_conf_vldt_array_iterator,
      (void *) &nxt_conf_vldt_route },

    { nxt_string("upstreams"),
      NXT_CONF_VLDT_OBJECT,
      &nxt_conf_vldt_object_iterator,
      (void *) &nxt_conf_vldt_upstream },

    NXT_CONF_VLDT_END
};


static nxt_conf_vldt_object_t  nxt_conf_vldt_match_members[] = {
    { nxt_string("method"),
      NXT_CONF_VLDT_STRING | NXT_CONF_VLDT_ARRAY,
      &nxt_conf_vldt_match_patterns,
      NULL },

    { nxt_string("scheme"),
      NXT_CONF_VLDT_STRING,
      &nxt_conf_vldt_match_scheme_pattern,
      NULL },

    { nxt_string("host"),
      NXT_CONF_VLDT_STRING | NXT_CONF_VLDT_ARRAY,
      &nxt_conf_vldt_match_patterns,
      NULL },

    { nxt_string("uri"),
      NXT_CONF_VLDT_STRING | NXT_CONF_VLDT_ARRAY,
      &nxt_conf_vldt_match_patterns,
      NULL },

    { nxt_string("arguments"),
      NXT_CONF_VLDT_OBJECT | NXT_CONF_VLDT_ARRAY,
      &nxt_conf_vldt_match_patterns_sets,
      NULL },

    { nxt_string("headers"),
      NXT_CONF_VLDT_OBJECT | NXT_CONF_VLDT_ARRAY,
      &nxt_conf_vldt_match_patterns_sets,
      NULL },

    { nxt_string("cookies"),
      NXT_CONF_VLDT_OBJECT | NXT_CONF_VLDT_ARRAY,
      &nxt_conf_vldt_match_patterns_sets,
      NULL },

    NXT_CONF_VLDT_END
};


static nxt_conf_vldt_object_t  nxt_conf_vldt_limit_conn_members[] = {
    { nxt_string("key"),
      NXT_CONF_VLDT_STRING,
      NULL,
      NULL },

    { nxt_string("conn"),
      NXT_CONF_VLDT_INTEGER,
      NULL,
      NULL },

    NXT_CONF_VLDT_END
};


static nxt_conf_vldt_object_t  nxt_conf_vldt_limit_req_members[] = {
    { nxt_string("key"),
      NXT_CONF_VLDT_STRING,
      NULL,
      NULL },

    { nxt_string("rate"),
      NXT_CONF_VLDT_INTEGER,
      NULL,
      NULL },

    { nxt_string("burst"),
      NXT_CONF_VLDT_INTEGER,
      NULL,
      NULL },

    NXT_CONF_VLDT_END
};


static nxt_conf_vldt_object_t  nxt_conf_vldt_action_members[] = {

    { nxt_string("variables"),
      NXT_CONF_VLDT_OBJECT,
      &nxt_conf_vldt_object_iterator,
      (void *) &nxt_conf_vldt_variable },

    { nxt_string("blacklist"),
      NXT_CONF_VLDT_STRING | NXT_CONF_VLDT_ARRAY,
      &nxt_conf_vldt_match_addrs,
      NULL },

    { nxt_string("whitelist"),
      NXT_CONF_VLDT_STRING | NXT_CONF_VLDT_ARRAY,
      &nxt_conf_vldt_match_addrs,
      NULL },

    { nxt_string("add_headers"),
      NXT_CONF_VLDT_OBJECT,
      &nxt_conf_vldt_object_iterator,
      (void *) &nxt_conf_vldt_add_header },

    { nxt_string("limit_conn"),
      NXT_CONF_VLDT_OBJECT,
      &nxt_conf_vldt_object,
      (void *) &nxt_conf_vldt_limit_conn_members },

    { nxt_string("limit_req"),
      NXT_CONF_VLDT_OBJECT,
      &nxt_conf_vldt_object,
      (void *) &nxt_conf_vldt_limit_req_members },

    { nxt_string("limit_rate"),
      NXT_CONF_VLDT_INTEGER,
      NULL,
      NULL },

    { nxt_string("return"),
      NXT_CONF_VLDT_INTEGER,
      NULL,
      NULL },

    { nxt_string("text"),
      NXT_CONF_VLDT_STRING,
      NULL,
      NULL },

    { nxt_string("location"),
      NXT_CONF_VLDT_STRING,
      NULL,
      NULL },

    NXT_CONF_VLDT_END
};


static nxt_conf_vldt_object_t  nxt_conf_vldt_route_members[] = {
    { nxt_string("match"),
      NXT_CONF_VLDT_OBJECT,
      &nxt_conf_vldt_object,
      (void *) &nxt_conf_vldt_match_members },

    { nxt_string("action"),
      NXT_CONF_VLDT_OBJECT,
      &nxt_conf_vldt_action,
      NULL },

    NXT_CONF_VLDT_END
};


static nxt_conf_vldt_object_t  nxt_conf_vldt_upstream_server_members[] = {
    { nxt_string("address"),
      NXT_CONF_VLDT_STRING,
      &nxt_conf_vldt_server_address,
      NULL },

    { nxt_string("weight"),
      NXT_CONF_VLDT_NUMBER,
      &nxt_conf_vldt_server_weight,
      NULL },

    { nxt_string("max_conns"),
      NXT_CONF_VLDT_NUMBER,
      &nxt_conf_vldt_server_max_conns,
      NULL },

    { nxt_string("max_fails"),
      NXT_CONF_VLDT_NUMBER,
      &nxt_conf_vldt_server_max_fails,
      NULL },

    { nxt_string("fail_timeout"),
      NXT_CONF_VLDT_NUMBER,
      &nxt_conf_vldt_server_fail_timeout,
      NULL },

    { nxt_string("down"),
      NXT_CONF_VLDT_BOOLEAN,
      NULL,
      NULL },

    NXT_CONF_VLDT_END
};


nxt_int_t
nxt_conf_validate(nxt_conf_validation_t *vldt)
{
    nxt_int_t  ret;

    ret = nxt_conf_vldt_type(vldt, NULL, vldt->conf, NXT_CONF_VLDT_OBJECT);

    if (ret != NXT_OK) {
        return ret;
    }

    return nxt_conf_vldt_object(vldt, vldt->conf, nxt_conf_vldt_root_members);
}


#define NXT_CONF_VLDT_ANY_TYPE                                                \
    "either a null, a boolean, an integer, "                                  \
    "a number, a string, an array, or an object"


static nxt_int_t
nxt_conf_vldt_type(nxt_conf_validation_t *vldt, nxt_str_t *name,
    nxt_conf_value_t *value, nxt_conf_vldt_type_t type)
{
    u_char      *p;
    nxt_str_t   expected;
    nxt_bool_t  serial;
    nxt_uint_t  value_type, n, t;
    u_char      buf[nxt_length(NXT_CONF_VLDT_ANY_TYPE)];

    static nxt_str_t  type_name[] = {
        nxt_string("a null"),
        nxt_string("a boolean"),
        nxt_string("an integer number"),
        nxt_string("a fractional number"),
        nxt_string("a string"),
        nxt_string("an array"),
        nxt_string("an object"),
    };

    value_type = nxt_conf_type(value);

    if ((1 << value_type) & type) {
        return NXT_OK;
    }

    p = buf;

    n = nxt_popcount(type);

    if (n > 1) {
        p = nxt_cpymem(p, "either ", 7);
    }

    serial = (n > 2);

    for ( ;; ) {
        t = __builtin_ffs(type) - 1;

        p = nxt_cpymem(p, type_name[t].start, type_name[t].length);

        n--;

        if (n == 0) {
            break;
        }

        if (n > 1 || serial) {
            *p++ = ',';
        }

        if (n == 1) {
            p = nxt_cpymem(p, " or", 3);
        }

        *p++ = ' ';

        type = type & ~(1 << t);
    }

    expected.length = p - buf;
    expected.start = buf;

    if (name == NULL) {
        return nxt_conf_vldt_error(vldt,
                                   "The configuration must be %V, but not %V.",
                                   &expected, &type_name[value_type]);
    }

    return nxt_conf_vldt_error(vldt,
                               "The \"%V\" value must be %V, but not %V.",
                               name, &expected, &type_name[value_type]);
}


static nxt_int_t
nxt_conf_vldt_error(nxt_conf_validation_t *vldt, const char *fmt, ...)
{
    u_char   *p, *end;
    size_t   size;
    va_list  args;
    u_char   error[NXT_MAX_ERROR_STR];

    va_start(args, fmt);
    end = nxt_vsprintf(error, error + NXT_MAX_ERROR_STR, fmt, args);
    va_end(args);

    size = end - error;

    p = nxt_mp_nget(vldt->pool, size);
    if (p == NULL) {
        return NXT_ERROR;
    }

    nxt_memcpy(p, error, size);

    vldt->error.length = size;
    vldt->error.start = p;

    return NXT_DECLINED;
}


static nxt_int_t
nxt_conf_vldt_object(nxt_conf_validation_t *vldt, nxt_conf_value_t *value,
    void *data)
{
    uint32_t                index;
    nxt_int_t               ret;
    nxt_str_t               name;
    nxt_conf_value_t        *member;
    nxt_conf_vldt_object_t  *vals;

    index = 0;

    for ( ;; ) {
        member = nxt_conf_next_object_member(value, &name, &index);

        if (member == NULL) {
            return NXT_OK;
        }

        vals = data;

        for ( ;; ) {
            if (vals->name.length == 0) {

                if (vals->data != NULL) {
                    vals = vals->data;
                    continue;
                }

                return nxt_conf_vldt_error(vldt, "Unknown parameter \"%V\".",
                                           &name);
            }

            if (!nxt_strstr_eq(&vals->name, &name)) {
                vals++;
                continue;
            }

            ret = nxt_conf_vldt_type(vldt, &name, member, vals->type);

            if (ret != NXT_OK) {
                return ret;
            }

            if (vals->validator != NULL) {
                ret = vals->validator(vldt, member, vals->data);

                if (ret != NXT_OK) {
                    return ret;
                }
            }

            break;
        }
    }
}


static nxt_int_t
nxt_conf_vldt_object_iterator(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    uint32_t                index;
    nxt_int_t               ret;
    nxt_str_t               name;
    nxt_conf_value_t        *member;
    nxt_conf_vldt_member_t  validator;

    validator = (nxt_conf_vldt_member_t) data;
    index = 0;

    for ( ;; ) {
        member = nxt_conf_next_object_member(value, &name, &index);

        if (member == NULL) {
            return NXT_OK;
        }

        ret = validator(vldt, &name, member);

        if (ret != NXT_OK) {
            return ret;
        }
    }
}


static nxt_int_t
nxt_conf_vldt_array_iterator(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    uint32_t                 index;
    nxt_int_t                ret;
    nxt_conf_value_t         *element;
    nxt_conf_vldt_element_t  validator;

    validator = (nxt_conf_vldt_element_t) data;

    for (index = 0; /* void */ ; index++) {
        element = nxt_conf_get_array_element(value, index);

        if (element == NULL) {
            return NXT_OK;
        }

        ret = validator(vldt, element);

        if (ret != NXT_OK) {
            return ret;
        }
    }
}


static nxt_int_t
nxt_conf_vldt_action(nxt_conf_validation_t *vldt, nxt_conf_value_t *value,
    void *data)
{
    nxt_int_t         ret;
    nxt_uint_t        status;
    nxt_conf_value_t  *return_value, *text_value, *location_value;

    static nxt_str_t  return_str = nxt_string("return");
    static nxt_str_t  text_str = nxt_string("text");
    static nxt_str_t  location_str = nxt_string("location");

    ret = nxt_conf_vldt_object(vldt, value, nxt_conf_vldt_action_members);
    if (nxt_slow_path(ret != NXT_OK)) {
        return ret;
    }

    return_value = nxt_conf_get_object_member(value, &return_str, NULL);
    
    if (return_value == NULL) {
        return NXT_OK;
    }

    status = nxt_conf_get_number(return_value);

    if (status < 100 || status > 599) {
        return nxt_conf_vldt_error(vldt, "The \"return\" number must be "
                                   "between 100 and 600.");
    }

    text_value = nxt_conf_get_object_member(value, &text_str, NULL);
    location_value = nxt_conf_get_object_member(value, &location_str, NULL);

    if (status == 301 || status == 302 || status == 303
         ||status == 307 || status == 308)
    {
        if (location_value == NULL) {
            return nxt_conf_vldt_error(vldt, "The \"location\" is required if "
                                       "\"return\" number is one of 301, 302, 303, 307 and 308.");
        }

    } else {
        if (text_value == NULL) {
            return nxt_conf_vldt_error(vldt, "The \"text\" is required if "
                                       "\"return\" number is not one of 301, 302, 303, 307 and 308.");
        }
    }

    if (text_value != NULL && location_value != NULL) {
        return nxt_conf_vldt_error(vldt, "The \"text\" and \"location\" can not set "
                                         "at the same time.");
    }

    return NXT_OK;
}


static nxt_int_t
nxt_conf_vldt_route(nxt_conf_validation_t *vldt, nxt_conf_value_t *value)
{
    if (nxt_conf_type(value) != NXT_CONF_OBJECT) {
        return nxt_conf_vldt_error(vldt, "The \"routes\" array must contain "
                                   "only object values.");
    }

    return nxt_conf_vldt_object(vldt, value, nxt_conf_vldt_route_members);
}


static nxt_int_t
nxt_conf_vldt_match_patterns(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    if (nxt_conf_type(value) == NXT_CONF_ARRAY) {
        return nxt_conf_vldt_array_iterator(vldt, value,
                                            &nxt_conf_vldt_match_pattern);
    }

    /* NXT_CONF_STRING */

    return nxt_conf_vldt_match_pattern(vldt, value);
}


static nxt_int_t
nxt_conf_vldt_match_pattern(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value)
{
    u_char      ch;
    nxt_str_t   pattern;
    nxt_uint_t  i, first, last;

    enum {
        sw_none,
        sw_side,
        sw_middle
    } state;

    if (nxt_conf_type(value) != NXT_CONF_STRING) {
        return nxt_conf_vldt_error(vldt, "The \"match\" patterns for \"host\", "
                                   "\"uri\", and \"method\" must be strings.");
    }

    nxt_conf_get_string(value, &pattern);

    if (pattern.length == 0) {
        return NXT_OK;
    }

    first = (pattern.start[0] == '!');
    last = pattern.length - 1;
    state = sw_none;

    for (i = first; i != pattern.length; i++) {

        ch = pattern.start[i];

        if (ch != '*') {
            continue;
        }

        switch (state) {
        case sw_none:
            state = (i == first) ? sw_side : sw_middle;
            break;

        case sw_side:
            if (i == last) {
                if (last - first != 1) {
                    break;
                }

                return nxt_conf_vldt_error(vldt, "The \"match\" pattern must "
                                           "not contain double \"*\" markers.");
            }

            /* Fall through. */

        case sw_middle:
            return nxt_conf_vldt_error(vldt, "The \"match\" patterns can "
                                       "either contain \"*\" markers at "
                                       "the sides or only one in the middle.");
        }
    }

    return NXT_OK;
}


static nxt_int_t
nxt_conf_vldt_match_scheme_pattern(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    nxt_str_t  scheme;

    static const nxt_str_t  http = nxt_string("http");
    static const nxt_str_t  https = nxt_string("https");

    nxt_conf_get_string(value, &scheme);

    if (nxt_strcasestr_eq(&scheme, &http)
        || nxt_strcasestr_eq(&scheme, &https))
    {
        return NXT_OK;
    }

    return nxt_conf_vldt_error(vldt, "The \"scheme\" can either be "
                                     "\"http\" or \"https\".");
}


static nxt_int_t
nxt_conf_vldt_match_addrs(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    if (nxt_conf_type(value) == NXT_CONF_ARRAY) {
        return nxt_conf_vldt_array_iterator(vldt, value,
                                            &nxt_conf_vldt_match_addr);
    }

    return nxt_conf_vldt_match_addr(vldt, value);
}


static nxt_int_t
nxt_conf_vldt_match_addr(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value)
{
    nxt_addr_pattern_t  pattern;

    switch (nxt_addr_pattern_parse(vldt->pool, &pattern, value)) {

    case NXT_OK:
        return NXT_OK;

    case NXT_ADDR_PATTERN_CV_TYPE_ERROR:
        return nxt_conf_vldt_error(vldt, "The \"match\" pattern for "
                                         "\"address\" must be a string.");

    case NXT_ADDR_PATTERN_LENGTH_ERROR:
        return nxt_conf_vldt_error(vldt, "The \"address\" is too short.");

    case NXT_ADDR_PATTERN_FORMAT_ERROR:
        return nxt_conf_vldt_error(vldt, "The \"address\" format is invalid.");

    case NXT_ADDR_PATTERN_RANGE_OVERLAP_ERROR:
        return nxt_conf_vldt_error(vldt, "The \"address\" range is "
                                         "overlapping.");

    case NXT_ADDR_PATTERN_CIDR_ERROR:
        return nxt_conf_vldt_error(vldt, "The \"address\" has an invalid CIDR "
                                         "prefix.");

    case NXT_ADDR_PATTERN_NO_IPv6_ERROR:
        return nxt_conf_vldt_error(vldt, "The \"address\" does not support "
                                         "IPv6 with your configuration.");

    default:
        return nxt_conf_vldt_error(vldt, "The \"address\" has an unknown "
                                         "format.");
    }
}


static nxt_int_t
nxt_conf_vldt_match_patterns_sets(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    if (nxt_conf_type(value) == NXT_CONF_ARRAY) {
        return nxt_conf_vldt_array_iterator(vldt, value,
                                            &nxt_conf_vldt_match_patterns_set);
    }

    /* NXT_CONF_OBJECT */

    return nxt_conf_vldt_match_patterns_set(vldt, value);
}


static nxt_int_t
nxt_conf_vldt_match_patterns_set(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value)
{
    if (nxt_conf_type(value) != NXT_CONF_OBJECT) {
        return nxt_conf_vldt_error(vldt, "The \"match\" patterns for "
                                   "\"arguments\", \"cookies\", and "
                                   "\"headers\" must be objects.");
    }

    return nxt_conf_vldt_object_iterator(vldt, value,
                                     &nxt_conf_vldt_match_patterns_set_member);
}


static nxt_int_t
nxt_conf_vldt_match_patterns_set_member(nxt_conf_validation_t *vldt,
    nxt_str_t *name, nxt_conf_value_t *value)
{
    if (name->length == 0) {
        return nxt_conf_vldt_error(vldt, "The \"match\" pattern objects must "
                                   "not contain empty member names.");
    }

    return nxt_conf_vldt_match_patterns(vldt, value, NULL);
}


static nxt_int_t
nxt_conf_vldt_variable(nxt_conf_validation_t *vldt, nxt_str_t *name,
    nxt_conf_value_t *value)
{
    if (name->length == 0) {
        return nxt_conf_vldt_error(vldt,
                                   "The variable name must not be empty.");
    }

    if (nxt_conf_type(value) != NXT_CONF_STRING) {
        return nxt_conf_vldt_error(vldt, "The \"%V\" variable value must be "
                                   "a string.", name);
    }

    return NXT_OK;
}


static nxt_int_t
nxt_conf_vldt_add_header(nxt_conf_validation_t *vldt, nxt_str_t *name,
    nxt_conf_value_t *value)
{
    if (name->length == 0) {
        return nxt_conf_vldt_error(vldt,
                                   "The add_header name must not be empty.");
    }

    if (nxt_conf_type(value) != NXT_CONF_STRING) {
        return nxt_conf_vldt_error(vldt, "The \"%V\" add_header value must be "
                                   "a string.", name);
    }

    return NXT_OK;
}


static nxt_int_t
nxt_conf_vldt_upstream(nxt_conf_validation_t *vldt, nxt_str_t *name,
    nxt_conf_value_t *value)
{
    nxt_int_t   ret;
    nxt_uint_t  n;

    ret = nxt_conf_vldt_type(vldt, name, value, NXT_CONF_VLDT_ARRAY);
    if (ret != NXT_OK) {
        return ret;
    }

    ret = nxt_conf_vldt_array_iterator(vldt, value, &nxt_conf_vldt_server);
    if (ret != NXT_OK) {
        return ret;
    }

    n = nxt_conf_array_elements_count(value);
    if (n == 0) {
        return nxt_conf_vldt_error(vldt, "The \"%V\" upstream must contain servers.");
    }

    return NXT_OK;
}


static nxt_int_t
nxt_conf_vldt_server(nxt_conf_validation_t *vldt, nxt_conf_value_t *value)
{
    if (nxt_conf_type(value) != NXT_CONF_OBJECT) {
        return nxt_conf_vldt_error(vldt, "The \"upstreams\" server value must be "
                                   "a object.");
    }

    return nxt_conf_vldt_object(vldt, value,
                                nxt_conf_vldt_upstream_server_members);
}


static nxt_int_t
nxt_conf_vldt_server_address(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    nxt_str_t       address;
    nxt_sockaddr_t  *sa;

    nxt_conf_get_string(value, &address);

    sa = nxt_sockaddr_parse(vldt->pool, &address);
    if (sa == NULL) {
        return nxt_conf_vldt_error(vldt, "The \"%V\" is not valid "
                                   "server address.", &address);
    }

    return NXT_OK;
}


static nxt_int_t
nxt_conf_vldt_server_weight(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    double  num_value;

    num_value = nxt_conf_get_number(value);

    if (num_value < 0) {
        return nxt_conf_vldt_error(vldt, "The \"weight\" number must be positive.");
    }

    if (num_value > 1000000) {
        return nxt_conf_vldt_error(vldt, "The \"weight\" number must "
                                   "not exceed 1,000,000.");
    }

    return NXT_OK;
}


static nxt_int_t
nxt_conf_vldt_server_max_conns(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    double  num_value;

    num_value = nxt_conf_get_number(value);

    if (num_value < 0) {
        return nxt_conf_vldt_error(vldt, "The \"max_conns\" number must be positive.");
    }

    if (num_value > 1000000) {
        return nxt_conf_vldt_error(vldt, "The \"max_conns\" number must "
                                   "not exceed 1000,000.");
    }

    return NXT_OK;
}


static nxt_int_t
nxt_conf_vldt_server_max_fails(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    double  num_value;

    num_value = nxt_conf_get_number(value);

    if (num_value < 0) {
        return nxt_conf_vldt_error(vldt, "The \"max_fails\" number must be positive.");
    }

    if (num_value > 1000000) {
        return nxt_conf_vldt_error(vldt, "The \"max_fails\" number must "
                                   "not exceed 1000,000.");
    }

    return NXT_OK;
}


static nxt_int_t
nxt_conf_vldt_server_fail_timeout(nxt_conf_validation_t *vldt,
    nxt_conf_value_t *value, void *data)
{
    double  num_value;

    num_value = nxt_conf_get_number(value);

    if (num_value < 0) {
        return nxt_conf_vldt_error(vldt, "The \"fail_timeout\" number must be positive.");
    }

    if (num_value > 1000000) {
        return nxt_conf_vldt_error(vldt, "The \"fail_timeout\" number must "
                                   "not exceed 1000,000.");
    }

    return NXT_OK;
}
