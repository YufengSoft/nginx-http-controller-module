
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_LIB_H_INCLUDED_
#define _NXT_LIB_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

#define NXT_INET6                   NGX_HAVE_INET6
#define NXT_PTR_SIZE                NGX_PTR_SIZE
#define NXT_HAVE_MEMALIGN           NGX_HAVE_MEMALIGN
#define NXT_HAVE_POSIX_MEMALIGN     NGX_HAVE_POSIX_MEMALIGN

#include <nxt_clang.h>
#include <nxt_types.h>
#include <nxt_malloc.h>
#include <nxt_mp.h>
#include <nxt_string.h>
#include <nxt_file.h>
#include <nxt_sockaddr.h>
#include <nxt_queue.h>
#include <nxt_rbtree.h>
#include <nxt_lvlhsh.h>
#include <nxt_list.h>
#include <nxt_array.h>
#include <nxt_djb_hash.h>
#include <nxt_utf8.h>
#include <nxt_parse.h>
#include <nxt_sprintf.h>
#include <nxt_conf.h>
#include <nxt_addr.h>


#endif /* _NXT_LIB_H_INCLUDED_ */
