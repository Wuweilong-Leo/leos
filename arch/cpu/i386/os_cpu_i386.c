#include "os_cpu.h"
#include "os_def.h"
#include "os_pgt.h"
#include "os_tss.h"
#include "os_task_external.h"
#include "os_reset.h"
#include "os_sched_external.h"
#include "os_process_external.h"
#include "os_debug_external.h"
#include "os_context_i386.h"
#include "os_mem_external.h"
#include "os_hwi.h"

OS_SEC_KERNEL_TEXT void OsSetContext(uintptr_t stkMemBase, size_t stkSize, struct OsTaskCb *tskCb)
{
    uintptr_t stkBot = stkMemBase + stkSize;
    struct OsFastSaveContext *fastSaveContext;

    stkBot -= sizeof(struct OsAllSaveContext);
    stkBot -= sizeof(struct OsFastSaveContext);

    fastSaveContext = (struct OsFastSaveContext *)stkBot;
    fastSaveContext->saveFlag = OS_FAST_SAVE_FLAG;
    fastSaveContext->eip = OsTaskCommonEntry;
    fastSaveContext->tskId = tskCb->pid;
    tskCb->stkPtr = stkBot;
}

OS_SEC_KERNEL_TEXT void OsProcessEntry(OsProcessEntryFunc entry, void *param1, void *param2)
{
    struct OsTaskCb *curTsk;
    enum OsIntStatus intSave;
    uintptr_t stkTop;
    struct OsAllSaveContext *allSaveContext;
    uintptr_t memBase;

    (void)param1;
    (void)param2;

    /* 当前还在内核态 */
    intSave = OsIntLock();

    curTsk = OS_RUNNING_TASK();

    /* 当前tcb里保存的栈顶指针还指向之前伪造的栈顶 */
    stkTop =
        (U32)curTsk->kernelStkTop + OS_TASK_KERNEL_STACK_SIZE - sizeof(struct OsAllSaveContext);

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
        OS_REBOOT("%s\n", "OsProcessEntry: OsMemUsrAllocPgByAddr failed");
    }

    allSaveContext->esp = (uintptr_t)((U32)memBase + OS_PG_SIZE);

    /* 通过中断返回切到进程，我们设置过eflags，因此切出去直接开中断 */
    OS_EMBED_ASM("mov %0, %%esp; jmp OsSwitch2Process" ::"g"((U32)allSaveContext) : "memory");
}

OS_SEC_KERNEL_TEXT void OsProcessInitArch(struct OsTaskCb *process)
{
    uintptr_t pgdir;

    pgdir = OsCreateProcessPgd();
    if (pgdir == NULL) {
        OS_REBOOT("%s\n", "OsProcessInitArch: OsCreateProcessPgd failed");
    }

    process->pgDir = pgdir;
}

OS_SEC_KERNEL_TEXT void OsConfigPgdForTskSwitch(struct OsTaskCb *tsk)
{
    uintptr_t pgdPhyAddr;

    if (tsk->tskType == OS_TASK_PROCESS) {
        /* 获取页目录的物理地址 */
        pgdPhyAddr = OsGetPaddrByVaddr(tsk->pgDir);
        // OS_DEBUG_KPRINT("OsConfigPgdForTskSwitch: pgdPhyAddr = 0x%x\n", (U32)pgdPhyAddr);
        OsLoadPgd(pgdPhyAddr);
    } else {
        OsLoadPgd(OS_KERNEL_PGD_BASE);
    }
}

OS_SEC_KERNEL_TEXT void OsConfigTssForTskSwitch(struct OsTaskCb *tsk)
{
    if (tsk->tskType == OS_TASK_PROCESS) {
        OsTssUpdateEsp0(OS_SELECTOR_K_DATA,
                        (uintptr_t)((U32)tsk->kernelStkTop + OS_TASK_KERNEL_STACK_SIZE));
    }
}

OS_SEC_KERNEL_TEXT void OsConfigArchForTskSwitch(struct OsTaskCb *tsk)
{
    OsConfigPgdForTskSwitch(tsk);

    /* 进入进程后，要把内核栈放到tss里存起来，因此要更新tss */
    OsConfigTssForTskSwitch(tsk);
}