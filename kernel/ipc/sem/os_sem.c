#include "os_sem_internal.h"
#include "os_debug_external.h"
#include "os_task_external.h"
#include "os_hwi_i386.h"
#include "os_sched_external.h"

OS_SEC_KERNEL_DATA struct OsList g_semFreeList = OS_LIST_INIT(g_semFreeList);  
OS_SEC_KERNEL_BSS struct OsSemCb g_semCbArray[OS_SEM_MAX_NUM];

OS_SEC_KERNEL_TEXT void OsSemConfig(void)
{
    U32 i;
    struct OsSemCb *semCb;
    struct OsList *freeListNode;

    for (i = 0; i < OS_SEM_MAX_NUM; i++) {
        semCb = &g_semCbArray[i];
        freeListNode = &semCb->freeListNode;

        semCb->semId = i;
        OsListInit(&semCb->pendList);
        OsListInit(&semCb->semListNode);
        OsListAddTail(&g_semFreeList, freeListNode);
    }
}

OS_INLINE struct OsSemCb *OsSemGetFreeCb(void)
{
    if (OsListIsEmpty(&g_semFreeList)) {
        return NULL;
    }

    return OS_LIST_GET_STRUCT_ENTRY(struct OsSemCb, freeListNode, 
                                    OS_LIST_GET_FIRST_NODE(&g_semFreeList));
}

OS_SEC_KERNEL_TEXT U32 OsSemCreate(U32 semCnt, U32 *semId)
{
    struct OsSemCb *semCb;

    semCb = OsSemGetFreeCb();
    if (semCb == NULL) {
        OS_DEBUG_KPRINT("%s", "OsSemCreate: OsSemGetFreeCb failed\n");
    }
    
    semCb->val = semCnt;
    semCb->semCnt = semCnt;
    *semId = semCb->semId;
    return OS_OK;
}

OS_INLINE bool OsSemIsHeldByTsk(struct OsSemCb *semCb, struct OsTaskCb *tsk)
{
    return OsListFindNode(&tsk->semList, &semCb->semListNode);
}

OS_SEC_KERNEL_TEXT U32 OsSemPend(U32 semId)
{
    struct OsSemCb *semCb;
    enum OsIntStatus intSave;
    struct OsTaskCb *curTsk;

    intSave = OsIntLock();

    semCb = OS_SEM_GET_CB(semId);
    curTsk = OS_RUNNING_TASK();

    /* 暂时不可重入 */
    if (OsSemIsHeldByTsk(semCb, curTsk)) {
        OsIntRestore(intSave);
        return OS_SEM_PEND_TSK_ALREADY_HOLD_SEM;
    }

    if (semCb->val == 0) {
        /* 加入到信号量pending队列 */
        OsListAddTail(&semCb->pendList, &curTsk->pendListNode);

        /* 从就绪队列里删除 */
        OsSchedDelTskFromRdyList(curTsk);
        curTsk->status = OS_TASK_SEM_PENDING;

        /* 触发调度 */
        OsTaskSchedule();
    }
    
    semCb->val--;
    OsListAddTail(&curTsk->semList, &semCb->semListNode);
    OsIntRestore(intSave);

    return OS_OK;
}

OS_INLINE bool OsSemHasPendingTsk(struct OsSemCb *semCb)
{
    return !OsListIsEmpty(&semCb->pendList);
}

OS_INLINE struct OsTaskCb *OsSemPopFirstPendingTsk(struct OsSemCb *semCb)
{
    struct OsList *pendListNode;
    
    pendListNode = OsListPopHead(&semCb->pendList);
    return OS_LIST_GET_STRUCT_ENTRY(struct OsTaskCb, pendListNode, 
                                    pendListNode);
}

OS_SEC_KERNEL_TEXT U32 OsSemPost(U32 semId)
{
    struct OsSemCb *semCb;
    enum OsIntStatus intSave;
    struct OsTaskCb *curTsk;
    struct OsTaskCb *pendTsk;

    intSave = OsIntLock();

    semCb = OS_SEM_GET_CB(semId);
    curTsk = OS_RUNNING_TASK();

    /* 没持有就释放是非法的 */
    if (!OsSemIsHeldByTsk(semCb, curTsk)) {
        OsIntRestore(intSave);
        return OS_SEM_POST_TSK_NOT_HOLD_SEM;
    }

    semCb->val++;
    /* 取消任务持有信号量 */
    OsListRemoveNode(&semCb->semListNode);

    if (OsSemHasPendingTsk(semCb)) {
        /* 有任务阻塞在此信号量 */
        /* 取出第一个信号量阻塞的任务 */ 
        pendTsk = OsSemPopFirstPendingTsk(semCb);
        /* 加回到就绪队列 */
        OsSchedAddTskToRdyListTail(pendTsk);
        pendTsk->status = OS_TASK_READY;

        /* 可能阻塞的是高优先级的任务，尝试触发调度 */
        OsTaskSchedule();
    }

    OsIntRestore(intSave);

    return OS_OK;
}