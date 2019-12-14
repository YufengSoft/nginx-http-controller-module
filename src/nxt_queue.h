
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) NGINX, Inc.
 */

#ifndef _NXT_QUEUE_H_INCLUDED_
#define _NXT_QUEUE_H_INCLUDED_


typedef struct nxt_queue_link_s  nxt_queue_link_t;

struct nxt_queue_link_s {
    nxt_queue_link_t  *prev;
    nxt_queue_link_t  *next;
};


typedef struct {
    nxt_queue_link_t  head;
} nxt_queue_t;


#define                                                                       \
nxt_queue_init(queue)                                                         \
    do {                                                                      \
        (queue)->head.prev = &(queue)->head;                                  \
        (queue)->head.next = &(queue)->head;                                  \
    } while (0)


#define                                                                       \
nxt_queue_is_empty(queue)                                                     \
    (&(queue)->head == (queue)->head.prev)

/*
 * A loop to iterate all queue links starting from head:
 *
 *      nxt_queue_link_t  link;
 *  } nxt_type_t  *tp;
 *
 *
 *  for (lnk = nxt_queue_first(queue);
 *       lnk != nxt_queue_tail(queue);
 *       lnk = nxt_queue_next(lnk))
 *  {
 *      tp = nxt_queue_link_data(lnk, nxt_type_t, link);
 *
 * or starting from tail:
 *
 *  for (lnk = nxt_queue_last(queue);
 *       lnk != nxt_queue_head(queue);
 *       lnk = nxt_queue_prev(lnk))
 *  {
 *      tp = nxt_queue_link_data(lnk, nxt_type_t, link);
 */

#define                                                                       \
nxt_queue_first(queue)                                                        \
    (queue)->head.next


#define                                                                       \
nxt_queue_last(queue)                                                         \
    (queue)->head.prev


#define                                                                       \
nxt_queue_head(queue)                                                         \
    (&(queue)->head)


#define                                                                       \
nxt_queue_tail(queue)                                                         \
    (&(queue)->head)


#define                                                                       \
nxt_queue_next(link)                                                          \
    (link)->next


#define                                                                       \
nxt_queue_prev(link)                                                          \
    (link)->prev


#define                                                                       \
nxt_queue_insert_head(queue, link)                                            \
    do {                                                                      \
        (link)->next = (queue)->head.next;                                    \
        (link)->next->prev = (link);                                          \
        (link)->prev = &(queue)->head;                                        \
        (queue)->head.next = (link);                                          \
    } while (0)


#define                                                                       \
nxt_queue_insert_tail(queue, link)                                            \
    do {                                                                      \
        (link)->prev = (queue)->head.prev;                                    \
        (link)->prev->next = (link);                                          \
        (link)->next = &(queue)->head;                                        \
        (queue)->head.prev = (link);                                          \
    } while (0)


#define                                                                       \
nxt_queue_insert_after(target, link)                                          \
    do {                                                                      \
        (link)->next = (target)->next;                                        \
        (link)->next->prev = (link);                                          \
        (link)->prev = (target);                                              \
        (target)->next = (link);                                              \
    } while (0)


#define                                                                       \
nxt_queue_insert_before(target, link)                                         \
    do {                                                                      \
        (link)->next = (target);                                              \
        (link)->prev = (target)->prev;                                        \
        (target)->prev = (link);                                              \
        (link)->prev->next = (link);                                          \
    } while (0)


#define                                                                       \
nxt_queue_remove(link)                                                        \
    do {                                                                      \
        (link)->next->prev = (link)->prev;                                    \
        (link)->prev->next = (link)->next;                                    \
    } while (0)


#define                                                                       \
nxt_queue_link_data(lnk, type, link)                                          \
    nxt_container_of(lnk, type, link)


#define nxt_queue_each(elt, queue, type, link)                                \
    do {                                                                      \
        nxt_queue_link_t  *_lnk, *_nxt;                                       \
                                                                              \
        for (_lnk = nxt_queue_first(queue);                                   \
             _lnk != nxt_queue_tail(queue);                                   \
             _lnk = _nxt) {                                                   \
                                                                              \
            _nxt = nxt_queue_next(_lnk);                                      \
            elt = nxt_queue_link_data(_lnk, type, link);                      \

#define nxt_queue_loop                                                        \
        }                                                                     \
    } while(0)


#endif /* _NXT_QUEUE_H_INCLUDED_ */
