
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_CLANG_H_INCLUDED_
#define _NXT_CLANG_H_INCLUDED_


#define nxt_inline     static inline __attribute__((always_inline))
#define nxt_noinline   __attribute__((noinline))
#define nxt_cdecl

#define NXT_EXPORT

#define NXT_MALLOC_LIKE

#define nxt_aligned(x)

#define nxt_packed

#define                                                                       \
nxt_expect(c, x)                                                              \
    (x)

#define                                                                       \
nxt_fast_path(x)                                                              \
    (x)

#define                                                                       \
nxt_slow_path(x)                                                              \
    (x)

#define                                                                       \
nxt_unreachable()

#define                                                                       \
nxt_prefetch(a)

#define                                                                       \
nxt_pragma_loop_disable_vectorization

#define                                                                       \
nxt_container_of(p, type, field)                                              \
    (type *) ((u_char *) (p) - offsetof(type, field))

#define nxt_pointer_to(p, offset)                                             \
    ((void *) ((char *) (p) + (offset)))

#define                                                                       \
nxt_nitems(x)                                                                 \
    (sizeof(x) / sizeof((x)[0]))

#define                                                                       \
nxt_max(val1, val2)                                                           \
    ((val1 < val2) ? (val2) : (val1))

#define                                                                       \
nxt_min(val1, val2)                                                           \
    ((val1 > val2) ? (val2) : (val1))

#define nxt_is_power_of_two(value)                                            \
    ((((value) - 1) & (value)) == 0)

#define                                                                       \
nxt_align_size(d, a)                                                          \
    (((d) + ((size_t) (a) - 1)) & ~((size_t) (a) - 1))

#define nxt_length(s)                                                         \
    (sizeof(s) - 1)

#define NXT_EAGAIN  EAGAIN

#define nxt_errno  errno

#define nxt_qsort  qsort

#define NXT_MAX_ERROR_STR  2048


#ifndef NXT_MAX_ALIGNMENT

#if (__i386__ || __i386)
#define NXT_MAX_ALIGNMENT  4

#elif (__arm__)
#define NXT_MAX_ALIGNMENT  16

#elif (__ia64__)
#define NXT_MAX_ALIGNMENT  16

#else
#define NXT_MAX_ALIGNMENT  16
#endif

#endif


nxt_inline int
nxt_popcount(unsigned int x)
{
    int  count;

    for (count = 0; x != 0; count++) {
        x &= x - 1;
    }

    return count;
}


#endif /* _NXT_CLANG_H_INCLUDED_ */
