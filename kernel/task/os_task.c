#include "os_sched_external.h"
#include "os_task_internal.h"
#include "os_def.h"
#include "os_list_external.h"
#include "os_mem_external.h"
#include "os_hwi_i386.h"
#include "os_context_i386.h"
#include "os_debug_external.h"
#include "os_sys.h"
#include "string.h"

OS_SEC_KERNEL_DATA struct OsTaskCb g_tskCbArray[OS_TASK_MAX_NUM];
OS_SEC_KERNEL_DATA struct OsList g_tskFreeList = OS_LIST_INIT(g_tskFreeList);

OS_SEC_KERNEL_TEXT void OsTaskConfig(void)
{
    U32 i;
    struct OsTaskCb *tskCb;

    for (i = 0; i < OS_TASK_MAX_NUM; i++) {
        tskCb = OS_TASK_GET_CB(i);

        tskCb->pid = i;
        tskCb->status = OS_TASK_NOT_CREATE;
        OsListAddTail(&g_tskFreeList, &tskCb->freeListNode);
    }
}

OS_SEC_KERNEL_TEXT void tsk1(void)
{
    while (1) {
        kprintf("tsk1\n");
    }
}

OS_SEC_KERNEL_TEXT void tsk2(void)
{
    while (1) {
        kprintf("tsk2\n");
    }
}

OS_SEC_KERNEL_TEXT void OsTaskIdleEntry(void)
{
    struct OsTaskCreateParam param = {0};
    U32 tskId;
    U32 ret;

    strcpy(param.name, "tsk1");
    param.prio = 0;
    param.entryFunc = tsk1;

    (void)OsTaskCreate(&param, &tskId);
    (void)OsTaskResume(tskId);

    strcpy(param.name, "tsk2");
    param.prio = 16;
    param.entryFunc = tsk2;

    (void)OsTaskCreate(&param, &tskId);
    (void)OsTaskResume(tskId);

    while (1) {
        kprintf("idle tsk\n");
    }
}

OS_INLINE struct OsTaskCb *OsTaskGetFreeCb(void)
{
    struct OsList *listNode;

    if (OsListIsEmpty(&g_tskFreeList)) {
        return NULL;
    }
    
    listNode = OsListPopHead(&g_tskFreeList);

    return OS_LIST_GET_STRUCT_ENTRY(struct OsTaskCb, freeListNode, listNode);
}

OS_SEC_KERNEL_TEXT void OsTaskEntry(U32 tskId)
{
    struct OsTaskCb *tskCb = OS_TASK_GET_CB(tskId);
    void **arg = tskCb->arg;

    /* 强制开中断 */
    (void)OsIntUnlock();
    tskCb->entry(arg[0], arg[1], arg[2], arg[3]);
    /* 强制关中断 */
    OsIntLock();
}

OS_INLINE void OsTaskSetCb(struct OsTaskCb *tskCb, struct OsTaskCreateParam *param)
{
    memcpy(tskCb->name, param->name, OS_TASK_NAME_MAX_SIZE);
    tskCb->entry = param->entryFunc;
    tskCb->prio = param->prio;
    tskCb->status = OS_TASK_NOT_RESUME;
    tskCb->arg[0] = param->arg[0];
    tskCb->arg[1] = param->arg[1];
    tskCb->arg[2] = param->arg[2];
    tskCb->arg[3] = param->arg[3];
}

OS_INLINE void OsTaskInitStack(uintptr_t stkBase, U32 stkSize)
{
    U32 i;

    for (i = 0; i < stkSize / 4; i++) {
        stkBase[i] = 0xCACACACA;
    }

    stkBase[0] = OS_TASK_STACK_TOP_MAGIC;
}

OS_SEC_KERNEL_TEXT U32 OsTaskCreate(struct OsTaskCreateParam * param, U32 *tskId)
{
    U32 ret;
    struct OsTaskCb *tskCb;
    uintptr_t stkMemBase;
    enum OsIntStatus intSave;

    intSave = OsIntLock();

    tskCb = OsTaskGetFreeCb();
    if (tskCb == NULL) {
        OS_DEBUG_KPRINT("%s\n", "OsTaskCreate: OsTaskGetFreeCb failed");
        OsIntRestore(intSave);
        return OS_TASK_CREATE_NO_FREE_CB;
    }
    
    OS_DEBUG_KPRINT("OsTaskCreate: freeCb = 0x%x\n", (U32)tskCb);

    stkMemBase = (uintptr_t)OsMemKernelAllocPgs(1);
    if (stkMemBase == NULL) {
        OsIntRestore(intSave);
        return OS_TASK_CREATE_STK_ALLOC_FAIL;
    }
    tskCb->kernelStkTop = stkMemBase;
    OS_DEBUG_KPRINT("OsTaskCreate: stkTop = 0x%x\n", tskCb->kernelStkTop);

    OsTaskInitStack(stkMemBase, OS_PG_SIZE);

    OsTaskSetCb(tskCb, param);
    OsSetContext(stkMemBase, OS_PG_SIZE, tskCb);
    tskCb->ticks = OsTaskGetInitialTick(tskCb->prio);

    *tskId = tskCb->pid;
    OS_DEBUG_KPRINT("OsTaskCreate: task pid = 0x%x\n", *tskId);

    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsTaskResume(U32 tskId)
{
    struct OsTaskCb *tskCb;
    enum OsIntStatus intSave;

    intSave = OsIntLock();
    tskCb = OS_TASK_GET_CB(tskId);
    if (tskCb->status != OS_TASK_NOT_RESUME) {
        return OS_TASK_RESUME_TSK_STATUS_ILL;
    }

    OsSchedAddTskToRdyListTail(tskCb);
    tskCb->status = OS_TASK_READY;

    OsTaskSchedule();

    OsIntRestore(intSave);
}

OS_INLINE void OsTaskMakeIdleRdy(struct OsTaskCb *idleTskCb)
{
    OsSchedAddTskToRdyListTail(idleTskCb);
    idleTskCb->status = OS_TASK_READY;
}

OS_SEC_KERNEL_TEXT U32 OsTaskCreateIdle(U32 *tskId)
{
    U32 ret;
    struct OsTaskCreateParam param = {0};
    struct OsTaskCb *idleTskCb;

    strcpy(param.name, "idle");
    param.prio = OS_TASK_LOWEST_PRIO;
    param.entryFunc = OsTaskIdleEntry;

    ret = OsTaskCreate(&param, tskId);
    if (ret != OS_OK) {
        return ret;
    }
    OS_DEBUG_KPRINT("idle tskId == 0x%x\n", *tskId);

    idleTskCb = OS_TASK_GET_CB(*tskId);

    OsTaskMakeIdleRdy(idleTskCb);

    return OS_OK;
}

OS_SEC_KERNEL_TEXT void OsTaskSchedule(void)
{
    if (!OS_RUN_QUE()->needSched) {
        return;
    }

    /* 在中断中，等中断尾部调度 */
    if (OS_HWI_ACTIVE(OS_RUN_QUE()->uniFlag)) {
        return;
    }

    OsTrapTsk(OS_RUNNING_TASK());
}
