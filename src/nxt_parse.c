
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>


/*
 * nxt_int_parse() returns size_t value >= 0 on success,
 * -1 on failure, and -2 on overflow.
 */

nxt_int_t
nxt_int_parse(const u_char *p, size_t length)
{
    u_char      c;
    nxt_uint_t  val;

    static const nxt_uint_t cutoff = NXT_INT_T_MAX / 10;
    static const nxt_uint_t cutlim = NXT_INT_T_MAX % 10;

    if (nxt_fast_path(length != 0)) {

        val = 0;

        do {
            c = *p++;

            /* Values below '0' become >= 208. */
            c = c - '0';

            if (nxt_slow_path(c > 9)) {
                return -1;
            }

            if (nxt_slow_path(val >= cutoff && (val > cutoff || c > cutlim))) {
                /* An overflow. */
                return -2;
            }

            val = val * 10 + c;

            length--;

        } while (length != 0);

        return val;
    }

    return -1;
}
