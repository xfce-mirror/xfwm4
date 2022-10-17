#ifndef __XFWM_LIST_H
#define __XFWM_LIST_H

#include <stddef.h>
#include <common/core-c.h>

struct list_head
{
    struct list_head* next;
    struct list_head* prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name);

#define LIST_POISON1 NULL
#define LIST_POISON2 NULL

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list->prev = list;
}

/**
 * list_entry_is_head - test if the entry points to the head of the list
 * @pos:        the type * to cursor
 * @head:       the head for your list.
 * @member:     the name of the list_head within the struct.
 */
#define list_entry_is_head(pos, head, member) \
    (&pos->member == (head))

/** raw list operations */

/**
 * list_add - add new entry ontop of a list (stack-like)
 * @new:        the new element
 * @head:       the head for your list
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    /* FIXME: could add a sanity check for already linked */
    new->prev = head;
    new->next = head->next;
    head->next = new;
}

/**
 * list_del - delete an entry from list. invalidates the entry
 * @entry:      the entry to delete
 */
static inline void list_del(struct list_head *entry)
{
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->prev = LIST_POISON1;
    entry->next = LIST_POISON1;
}

/**
 * list_empty - check whether a list is empty
 * @head:       the head for your list
 */
static inline int list_empty(struct list_head *head)
{
    return (head->next == head);
}

/**
 * iterate through all list entries
 * @pos:        cursor variable (type struct list_head)
 * @head:       the head of your list
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/* list entries */

/**
 * list_entry - get the container struct for this entry
 * @ptr:        the &struct list_head pointer.
 * @type:       the type of the struct this is embedded in.
 * @member:     the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/**
 * list_first_entry - get the first element from a list
 * @ptr:        the list head to take the element from.
 * @type:       the type of the struct this is embedded in.
 * @member:     the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/**
 * list_entry_is_head - test if the entry points to the head of the list
 * @pos:        the type * to cursor
 * @head:       the head for your list.
 * @member:     the name of the list_head within the struct.
 */
#define list_entry_is_head(pos, head, member) \
    (&pos->member == (head))

/**
 * list_next_entry - get the next element in list
 * @pos:        the type * to cursor
 * @member:     the name of the list_head within the struct.
 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_for_each_entry - iterate over list of given type
 * @pos:        the type * to use as a loop cursor.
 * @head:       the head for your list.
 * @member:     the name of the list_head within the struct.
 */
#define list_for_each_entry(pos, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:        the type * to use as a loop cursor.
 * @n:          another type * to use as temporary storage
 * @head:       the head for your list.
 * @member:     the name of the list_head within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)                  \
        for (pos = list_first_entry(head, typeof(*pos), member),        \
                n = list_next_entry(pos, member);                       \
             !list_entry_is_head(pos, head, member);                    \
             pos = n, n = list_next_entry(n, member))

#endif /* _XFWM_LIST_H */
