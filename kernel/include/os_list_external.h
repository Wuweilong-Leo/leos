#ifndef OS_LIST_EXTERNAL_H
#define OS_LIST_EXTERNAL_H
#include "os_def.h"

struct OsList {
    struct OsList *prev;
    struct OsList *next;
};

/* 获取结构体内元素的偏移 */
#define OFFSET(structType, elem) ((U32)(&(((structType *)0)->elem)))

/* 通过元素地址获取结构体的首地址 */
#define OS_LIST_GET_STRUCT_ENTRY(structType, elemName, elemAddr)               \
  ((structType *)((U32)(elemAddr) - OFFSET(structType, elemName)))

#define OS_LIST_FOR_EACH(list, tmpNode)                                        \
  for ((tmpNode) = (list)->next; (tmpNode) != (list); (tmpNode) = (tmpNode)->next)

#define OS_LIST_GET_FIRST_NODE(list) ((list)->next)

#define OS_LIST_INIT(list) {&(list), &(list)}

OS_INLINE void OsListInit(struct OsList *list)
{
    list->prev = list;
    list->next = list;
}

OS_INLINE void OsListInsertPrev(struct OsList *target, struct OsList *nextNode)
{
    nextNode->prev->next = target;
    target->prev = nextNode->prev;
    nextNode->prev = target;
    target->next = nextNode;
}

OS_INLINE void OsListAddTail(struct OsList *list, struct OsList *node)
{
    OsListInsertPrev(node, list);
}

OS_INLINE void OsListAddHead(struct OsList *list, struct OsList *node)
{
    OsListInsertPrev(node, list->next);
}

OS_INLINE void OsListRemoveNode(struct OsList *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    OsListInit(node);
}

OS_INLINE bool OsListIsEmpty(struct OsList *list)
{
    return list->next == list;
}

OS_INLINE struct OsList *OsListPopHead(struct OsList *list)
{
    struct OsList *firstNode;

    firstNode = OS_LIST_GET_FIRST_NODE(list);
    
    OsListRemoveNode(firstNode);

    return firstNode;
}

OS_INLINE bool OsListFindNode(struct OsList *list, struct OsList *node)
{
    struct OsList *tmpNode;

    OS_LIST_FOR_EACH(list, tmpNode) {
        if (node == tmpNode) {
            return TRUE;
        }
    }
    
    return FALSE;
}
#endif