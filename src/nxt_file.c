
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>


nxt_int_t
nxt_file_open(nxt_file_t *file, nxt_uint_t mode, nxt_uint_t create,
    nxt_file_access_t access)
{
#ifdef __CYGWIN__
    mode |= O_BINARY;
#endif

    /* O_NONBLOCK is to prevent blocking on FIFOs, special devices, etc. */
    mode |= (O_NONBLOCK | create);

    file->fd = open((char *) file->name, mode, access);

    file->error = (file->fd == -1) ? nxt_errno : 0;

    if (file->fd != -1) {
        return NXT_OK;
    }

    return NXT_ERROR;
}


nxt_int_t
nxt_file_info(nxt_file_t *file, nxt_file_info_t *fi)
{
    int  n;

    if (file->fd == NXT_FILE_INVALID) {
        n = stat((char *) file->name, fi);

        file->error = (n != 0) ? nxt_errno : 0;

        if (n == 0) {
            return NXT_OK;
        }

        return NXT_ERROR;

    } else {
        n = fstat(file->fd, fi);

        file->error = (n != 0) ? nxt_errno : 0;

        if (n == 0) {
            return NXT_OK;
        }

        return NXT_ERROR;
    }
}


ssize_t
nxt_file_read(nxt_file_t *file, u_char *buf, size_t size, nxt_off_t offset)
{
    ssize_t  n;

    n = pread(file->fd, buf, size, offset);

    file->error = (n <= 0) ? nxt_errno : 0;

    if (nxt_fast_path(n >= 0)) {
        return n;
    }

    return NXT_ERROR;
}


ssize_t
nxt_file_write(nxt_file_t *file, const u_char *buf, size_t size,
    nxt_off_t offset)
{
    ssize_t  n;

    n = pwrite(file->fd, buf, size, offset);

    file->error = (n < 0) ? nxt_errno : 0;

    if (nxt_fast_path(n >= 0)) {
        return n;
    }

    return NXT_ERROR;
}
