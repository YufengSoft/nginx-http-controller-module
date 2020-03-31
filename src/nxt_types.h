
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_TYPES_H_INCLUDED_
#define _NXT_TYPES_H_INCLUDED_


#define NXT_INET6                   NGX_HAVE_INET6
#define NXT_HAVE_UNIX_DOMAIN        NGX_HAVE_UNIX_DOMAIN
#define NXT_PTR_SIZE                NGX_PTR_SIZE
#define NXT_HAVE_MEMALIGN           NGX_HAVE_MEMALIGN
#define NXT_HAVE_POSIX_MEMALIGN     NGX_HAVE_POSIX_MEMALIGN


/*
 * nxt_int_t corresponds to the most efficient integer type,
 * an architecture word.  It is usually the long type,
 * but on Win64 the long is int32_t, so pointer size suits better.
 * nxt_int_t must be no less than int32_t.
 */

#if (__amd64__)
/*
 * AMD64 64-bit multiplication and division operations
 * are slower and 64-bit instructions are longer.
 */
#define NXT_INT_T_SIZE       4
typedef int                  nxt_int_t;
typedef u_int                nxt_uint_t;

#else
#define NXT_INT_T_SIZE       NXT_PTR_SIZE
typedef intptr_t             nxt_int_t;
typedef uintptr_t            nxt_uint_t;
#endif

typedef nxt_uint_t           nxt_bool_t;

typedef off_t                nxt_off_t;

typedef int                  nxt_err_t;


#if (NXT_PTR_SIZE == 8)
#define NXT_64BIT            1
#define NXT_32BIT            0

#else
#define NXT_64BIT            0
#define NXT_32BIT            1
#endif


#define NXT_INT64_T_LEN      nxt_length("-9223372036854775808")
#define NXT_INT32_T_LEN      nxt_length("-2147483648")

#define NXT_INT64_T_MAX      0x7FFFFFFFFFFFFFFFLL
#define NXT_INT32_T_MAX      0x7FFFFFFF


#if (NXT_INT_T_SIZE == 8)
#define NXT_INT_T_LEN        NXT_INT64_T_LEN
#define NXT_INT_T_MAX        NXT_INT64_T_MAX

#else
#define NXT_INT_T_LEN        NXT_INT32_T_LEN
#define NXT_INT_T_MAX        NXT_INT32_T_MAX
#endif


#define NXT_OK                     0
#define NXT_ERROR                  (-1)
#define NXT_AGAIN                  (-2)
#define NXT_DECLINED               (-3)
#define NXT_DONE                   (-4)

#define NXT_ERANGE                 ERANGE


#define NXT_LOG_ERR  NGX_LOG_ERR

#define nxt_thread_log_error(_level, ...)                                     \
        ngx_log_error(_level, ngx_cycle->log, 0, __VA_ARGS__);


#endif /* _NXT_TYPES_H_INCLUDED_ */
