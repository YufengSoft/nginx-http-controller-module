
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_UNIX_FILE_H_INCLUDED_
#define _NXT_UNIX_FILE_H_INCLUDED_


typedef int                         nxt_fd_t;
typedef u_char                      nxt_file_name_t;
typedef nxt_uint_t                  nxt_file_access_t;
typedef struct stat                 nxt_file_info_t;

#define NXT_FILE_INVALID            -1


typedef struct {
    nxt_file_name_t                 *name;
    nxt_fd_t                        fd;
    nxt_err_t                       error;
    nxt_off_t                       size;
} nxt_file_t;


/* The file open access modes. */
#define NXT_FILE_RDONLY             O_RDONLY
#define NXT_FILE_WRONLY             O_WRONLY
#define NXT_FILE_RDWR               O_RDWR
#define NXT_FILE_APPEND             (O_WRONLY | O_APPEND)

/* The file creation modes. */
#define NXT_FILE_CREATE_OR_OPEN     O_CREAT
#define NXT_FILE_OPEN               0
#define NXT_FILE_TRUNCATE           (O_CREAT | O_TRUNC)

/* The file access rights. */
#define NXT_FILE_DEFAULT_ACCESS     0644
#define NXT_FILE_OWNER_ACCESS       0600


#define                                                                       \
nxt_is_file(fi)                                                               \
    (S_ISREG((fi)->st_mode))


#define                                                                       \
nxt_file_size(fi)                                                             \
    (fi)->st_size



NXT_EXPORT nxt_int_t nxt_file_open(nxt_file_t *file, nxt_uint_t mode,
    nxt_uint_t create, nxt_file_access_t access);
NXT_EXPORT nxt_int_t nxt_file_info(nxt_file_t *file, nxt_file_info_t *fi);
NXT_EXPORT ssize_t nxt_file_read(nxt_file_t *file, u_char *buf, size_t size,
    nxt_off_t offset);
NXT_EXPORT ssize_t nxt_file_write(nxt_file_t *file, const u_char *buf,
    size_t size, nxt_off_t offset);


#endif /* _NXT_UNIX_FILE_H_INCLUDED_ */
