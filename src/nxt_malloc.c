
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>


void *
nxt_malloc(size_t size)
{
    void  *p;

    p = malloc(size);

    return p;
}


void *
nxt_zalloc(size_t size)
{
    void  *p;

    p = nxt_malloc(size);

    if (nxt_fast_path(p != NULL)) {
        nxt_memzero(p, size);
    }

    return p;
}


void *
nxt_realloc(void *p, size_t size)
{
    void  *n;

    n = realloc(p, size);

    return n;
}


#if (NXT_HAVE_POSIX_MEMALIGN)

/*
 * posix_memalign() presents in Linux glibc 2.1.91, FreeBSD 7.0,
 * Solaris 11, MacOSX 10.6 (Snow Leopard), NetBSD 5.0.
 */

void *
nxt_memalign(size_t alignment, size_t size)
{
    void        *p;
    nxt_err_t   err;

    err = posix_memalign(&p, alignment, size);

    if (nxt_fast_path(err == 0)) {
        return p;
    }

    return NULL;
}

#elif (NXT_HAVE_MEMALIGN)

/* memalign() presents in Solaris, HP-UX. */

void *
nxt_memalign(size_t alignment, size_t size)
{
    void  *p;

    p = memalign(alignment, size);

    if (nxt_fast_path(p != NULL)) {
        return p;
    }

    return NULL;
}

#else

#error no memalign() implementation.

#endif
