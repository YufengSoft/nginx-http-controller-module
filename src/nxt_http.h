
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_HTTP_H_INCLUDED_
#define _NXT_HTTP_H_INCLUDED_


typedef struct {
    nxt_mp_t                        *mem_pool;

    nxt_str_t                       method;
    nxt_str_t                       host;
    nxt_str_t                       path;
    nxt_str_t                       args;
    nxt_str_t                       body;

    nxt_array_t                     *arguments;  /* of nxt_http_name_value_t */
    nxt_list_t                      *fields;     /* of nxt_http_field_t */

    nxt_sockaddr_t                  *remote;
} nxt_http_request_t;


typedef struct {
    uint16_t                        hash;
    uint8_t                         skip:1;
    uint8_t                         hopbyhop:1;
    uint8_t                         name_length;
    uint32_t                        value_length;
    u_char                          *name;
    u_char                          *value;
} nxt_http_field_t;


typedef struct {
    uint16_t                        hash;
    uint16_t                        name_length;
    uint32_t                        value_length;
    u_char                          *name;
    u_char                          *value;
} nxt_http_name_value_t;


nxt_array_t *nxt_http_arguments_parse(nxt_http_request_t *r);


#define NXT_HTTP_FIELD_HASH_INIT        159406U
#define nxt_http_field_hash_char(h, c)  (((h) << 4) + (h) + (c))
#define nxt_http_field_hash_end(h)      (((h) >> 16) ^ (h))


#endif /* _NXT_HTTP_H_INCLUDED_ */
