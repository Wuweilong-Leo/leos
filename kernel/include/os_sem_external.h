#ifndef OS_SEM_EXTERNAL_H
#define OS_SEM_EXTERNAL_H
#include "os_def.h"
#include "os_list_external.h"
struct OsSemCb {
    U32 semId;
    U32 val;
    U32 semCnt;
    struct OsList freeListNode;
    /* 阻塞在此信号量的任务 */
    struct OsList pendList;
    /* 被任务持有的链表节点, taskCb->semList */
    struct OsList semListNode;
};

extern U32 OsSemCreate(U32 val, U32 *semId);
extern U32 OsSemPend(U32 semId);
extern U32 OsSemPost(U32 semId);
extern void OsSemConfig(void);
#endif