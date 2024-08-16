#include "os_process_internal.h"
#include "os_task_external.h"
#include "os_hwi_i386.h"
#include "string.h"
#include "os_sched_external.h"
#include "os_context_i386.h"
#include "os_cpu_i386.h"
#include "os_mem_external.h"
#include "os_debug_external.h"

OS_SEC_KERNEL_TEXT void OsProcessEntry(OsProcessEntryFunc entry, void *param1, void *param2)
{
    struct OsTaskCb *curTsk;
    enum OsIntStatus intSave;
    uintptr_t stkTop;
    struct OsAllSaveContext *allSaveContext;
    uintptr_t memBase;

    /* 当前还在内核态 */
    intSave = OsIntLock();

    curTsk = OS_RUNNING_TASK();

    /* 当前tcb里保存的栈顶指针还指向之前伪造的栈顶 */
    stkTop = (U32)curTsk->kernelStkTop + OS_TASK_KERNEL_STACK_SIZE - sizeof(struct OsAllSaveContext);

    allSaveContext = (struct OsAllSaveContext *)stkTop;
    allSaveContext->saveFlag = OS_ALL_SAVE_FLAG;
    allSaveContext->edi = 0;
    allSaveContext->esi = 0;
    allSaveContext->ebp = 0;
    allSaveContext->espDummy = 0;
    allSaveContext->eax = 0;
    allSaveContext->ebx = 0;
    allSaveContext->ecx = 0;
    allSaveContext->edx = 0;
    allSaveContext->gs = 0;
    allSaveContext->ds = OS_SELECTOR_U_DATA;
    allSaveContext->es = OS_SELECTOR_U_DATA;
    allSaveContext->fs = OS_SELECTOR_U_DATA;
    allSaveContext->ss = OS_SELECTOR_U_DATA;
    allSaveContext->cs = OS_SELECTOR_U_CODE;
    allSaveContext->eip = (uintptr_t)entry;
    allSaveContext->eflags = OS_PROCESS_EFLAGS;

    /* 创建用户栈 */
    memBase = OsMemUsrAllocPgByAddr((uintptr_t)OS_PROCESS_USR_STACK_BASE);
    if (memBase == NULL) {
        /* 申请失败直接挂死 */
        OS_DEBUG_KPRINT("%s\n", "OsProcessEntry: OsMemUsrAllocPgByAddr failed");
        while (1) {}
    }
    
    allSaveContext->esp = (uintptr_t)((U32)memBase + OS_PG_SIZE);

    /* 通过中断返回切到进程，我们设置过eflags，因此切出去直接开中断 */
    OS_EMBED_ASM("mov %0, %%esp; jmp OsSwitch2Process"::"g"((U32)allSaveContext):"memory");
}

static OS_SEC_KERNEL_TEXT void OsProcessInitVirMemPool(struct OsTaskCb *process)
{
    uintptr_t btmpBase;
    U32 usrMemBtmpPgNum;

    usrMemBtmpPgNum = OS_BTMP_GET_PG_NUM_BY_MEM_SIZE(OS_USR_VIR_MEM_SIZE); 
    OS_DEBUG_KPRINT("OsProcessInitVirMemPool: usrMemBtmpPgNum = 0x%x\n", usrMemBtmpPgNum);
    btmpBase = OsMemKernelAllocPgs(usrMemBtmpPgNum);
    if (btmpBase == NULL) {
        OS_DEBUG_KPRINT("%s\n", "OsProcessInitVirMemPool: OsMemKernelAllocPgs failed");
        while (1) {}
    }

    OsMemPoolInit(&process->usrVirMemPool, (uintptr_t)OS_USR_MEM_VIR_ADDR_START, 
                  OS_USR_VIR_MEM_SIZE, (U8 *)btmpBase);

}

OS_SEC_KERNEL_TEXT U32 OsProcessCreate(struct OsProcessCreateParam *processParam,  U32 *pid)
{
    U32 ret;
    U32 tskId;
    enum OsIntStatus intSave;
    struct OsTaskCreateParam tskParam;
    struct OsTaskCb *tskCb;
    
    tskParam.entryFunc = (OsTaskEntryFunc)OsProcessEntry;
    strcpy(tskParam.name, processParam->processName);
    /* 把进程入口作为第一个参数 */
    tskParam.arg[0] = (uintptr_t)processParam->entryFunc;
    /* 进程只能接受两个参数 */
    tskParam.arg[1] = processParam->param[0];
    tskParam.arg[2] = processParam->param[1];
    tskParam.prio = processParam->prio;

    intSave = OsIntLock();

    ret = OsTaskCreate(&tskParam, &tskId);
    if (ret != OS_OK) {
        OsIntRestore(intSave);
        return ret;
    }

    tskCb = &g_tskCbArray[tskId];

    /* 创建进程自己的页目录 */
    OsProcessInitArch(tskCb);

    /* 初始化进程虚拟内存池 */
    OsProcessInitVirMemPool(tskCb);

    tskCb->tskType = OS_TASK_PROCESS;
    *pid = tskId;

    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsProcessResume(U32 processId)
{
    return OsTaskResume(processId);
}