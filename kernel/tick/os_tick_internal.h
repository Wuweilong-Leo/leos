#ifndef OS_TICK_EXTERNAL_H
#define OS_TICK_EXTERNAL_H
#include "os_def.h"
#include "os_list_external.h"
extern U32 g_noRespondTicks;
#define OS_INC_NO_RESPOND_TICKS() (g_noRespondTicks++)
#define OS_INC_UNI_TICKS() (g_uniTicks++)

#define OS_POP_FIRST_TSK_FROM_DLY_LIST(dlyList) \
    OS_LIST_GET_STRUCT_ENTRY(struct OsTaskCb, dlyListNode, OsListPopHead(dlyList))

#define OS_GET_FIRST_TSK_IN_DLY_LIST(dlyList) \
    OS_LIST_GET_STRUCT_ENTRY(struct OsTaskCb, dlyListNode, OS_LIST_GET_FIRST_NODE(dlyList))

#endif