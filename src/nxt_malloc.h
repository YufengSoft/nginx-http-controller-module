
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_UNIX_MALLOC_H_INCLUDED_
#define _NXT_UNIX_MALLOC_H_INCLUDED_


NXT_EXPORT void *nxt_malloc(size_t size)
    NXT_MALLOC_LIKE;
NXT_EXPORT void *nxt_zalloc(size_t size)
    NXT_MALLOC_LIKE;
NXT_EXPORT void *nxt_realloc(void *p, size_t size)
    NXT_MALLOC_LIKE;
NXT_EXPORT void *nxt_memalign(size_t alignment, size_t size)
    NXT_MALLOC_LIKE;


#define                                                                       \
nxt_free(p)                                                                   \
    free(p)


#endif /* _NXT_UNIX_MALLOC_H_INCLUDED_ */
