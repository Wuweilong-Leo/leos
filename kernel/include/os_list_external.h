#ifndef OS_LIST_EXTERNAL_H
#define OS_LIST_EXTERNAL_H
#include "os_def.h"

struct os_list {
  struct os_list *prev;
  struct os_list *next;
};

/* 获取结构体内元素的偏移 */
#define OFFSET(struct_type, elem) ((U32)(&(((struct_type *)0)->elem)))

/* 通过元素地址获取结构体的首地址 */
#define OS_LIST_GET_STRUCT_ENTRY(struct_type, elem_name, elem_addr)               \
  ((struct_type *)((uint32_t)elem_addr - OFFSET(struct_type, elem_name)))

#define OS_LIST_FOR_EACH(list, tmp_node)                                        \
  for ((tmp_node) = (list)->next; (tmp_node) != (list); (tmp_node) = (tmp_node)->next)

#define OS_LIST_GET_FIRST_NODE(list) ((list)->next)

#define OS_LIST_INIT(list) {(list), (list)}

INLINE void os_list_init(struct os_list *list)
{
    list->prev = list;
    list->next = list;
}

INLINE void os_list_insert_prev(struct os_list *target, struct os_list *next_node)
{
    next_node->prev->next = target;
    target->prev = next_node->prev;
    next_node->prev = target;
    target->next = next_node;
}

INLINE void os_list_add_tail(struct os_list *list, struct os_list *node)
{
    os_list_insert_prev(node, list);
}

INLINE void os_list_add_head(struct os_list *list, struct os_list *node)
{
    os_list_insert_prev(node, list->next);
}

INLINE void os_list_remove_node(struct os_list *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}

INLINE struct os_list *os_list_pop_head(struct os_list *list)
{
    struct os_list *first_node = OS_LIST_GET_FIRST_NODE(list);

    os_list_remove_node(first_node);

    return first_node;
}

INLINE bool os_list_is_empty(struct os_list *list)
{
    return list->next != list;
}

INLINE bool os_list_find_node(struct os_list *list, struct os_list *node)
{
    struct os_list *tmp_node;

    OS_LIST_FOR_EACH(list, tmp_node) {
        if (node == tmp_node) {
            return TRUE;
        }
    }
    
    return FALSE;
}
#endif