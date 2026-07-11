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
#include "os_btmp_external.h"
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

    /* 当前还在内核态 */
    intSave = OsIntLock();

    curTsk = OS_RUNNING_TASK();

    /* 当前tcb里保存的栈顶指针还指向之前伪造的栈顶 */
    stkTop =
        (uintptr_t)curTsk->kernelStkTop + OS_TASK_KERNEL_STACK_SIZE - sizeof(struct OsAllSaveContext);

    allSaveContext = (struct OsAllSaveContext *)stkTop;
    allSaveContext->saveFlag = OS_ALL_SAVE_FLAG;
    allSaveContext->edi = 0;
    allSaveContext->esi = 0;
    allSaveContext->ebp = 0;
    allSaveContext->espDummy = 0;
    allSaveContext->eax = 0;
    allSaveContext->ebx = (U32)(uintptr_t)param1;
    allSaveContext->ecx = (U32)(uintptr_t)param2;
    allSaveContext->edx = 0;
    allSaveContext->gs = 0;
    allSaveContext->ds = OS_SELECTOR_U_DATA;
    allSaveContext->es = OS_SELECTOR_U_DATA;
    allSaveContext->fs = OS_SELECTOR_U_DATA;
    allSaveContext->ss = OS_SELECTOR_U_DATA;
    allSaveContext->cs = OS_SELECTOR_U_CODE;
    allSaveContext->eip = (uintptr_t)entry;
    allSaveContext->eflags = OS_PROCESS_EFLAGS;

    /* 用户栈已在 OsProcessCreate 中分配并映射到进程页表 */
    memBase = (uintptr_t)OS_PROCESS_USR_STACK_BASE;

    allSaveContext->esp = memBase + OS_PG_SIZE;

    /* 通过中断返回切到进程，我们设置过eflags，因此切出去直接开中断 */
    OS_EMBED_ASM("mov %0, %%esp; jmp OsSwitch2Process" ::"g"((uintptr_t)allSaveContext) : "memory");
}

OS_SEC_KERNEL_TEXT void OsProcessInitArch(struct OsTaskCb *process)
{
    uintptr_t pgdir;

    pgdir = OsCreateProcessPgd();
    if (pgdir == NULL) {
        OS_PANIC("OsCreateProcessPgd failed\n");
    }

    process->pgDir = pgdir;
}

OS_SEC_KERNEL_TEXT void OsConfigPgdForTskSwitch(struct OsTaskCb *tsk)
{
    uintptr_t pgdPhyAddr;

    if (tsk->tskType == OS_TASK_PROCESS) {
        /* 获取页目录的物理地址 */
        pgdPhyAddr = OsGetPaddrByVaddr(tsk->pgDir);
        OsLoadPgd(pgdPhyAddr);
    } else {
        OsLoadPgd(OS_KERNEL_PGD_BASE);
    }
}

OS_SEC_KERNEL_TEXT void OsConfigTssForTskSwitch(struct OsTaskCb *tsk)
{
    OsTssUpdateEsp0(OS_SELECTOR_K_DATA,
                    tsk->kernelStkTop + OS_TASK_KERNEL_STACK_SIZE);
}

OS_SEC_KERNEL_TEXT void OsConfigArchForTskSwitch(struct OsTaskCb *tsk)
{
    OsConfigPgdForTskSwitch(tsk);

    /* 进入进程后，要把内核栈放到tss里存起来，因此要更新tss */
    OsConfigTssForTskSwitch(tsk);
}

/* 释放进程的页表和页目录（i386 自映射实现） */
OS_SEC_KERNEL_TEXT void OsProcessFreeArchResources(struct OsTaskCb *tskCb)
{
    U32 pdeIdx, pteIdx;
    uintptr_t pdeVaddr, pteVaddr, virAddr, phyAddr;
    U32 phyIdx, virIdx;

    /* 切到进程页目录，才能通过自映射访问进程的 PT */
    OsLoadPgd(OsGetPaddrByVaddr(tskCb->pgDir));

    /* 遍历用户区 PDE，释放每个用户物理页和 PT 页 */
    for (pdeIdx = 0; pdeIdx < OS_PGD_KERNEL_IDX_START; pdeIdx++) {
        pdeVaddr = 0xFFFFF000 + pdeIdx * 4;
        if (!OsPdeIsExisted(pdeVaddr)) {
            continue;
        }

        /* 遍历该 PT 的所有 PTE，释放用户物理页 */
        for (pteIdx = 0; pteIdx < OS_PGT_ENTRY_NUM; pteIdx++) {
            pteVaddr = 0xFFC00000 + pdeIdx * 0x1000 + pteIdx * 4;
            if (!OsPteIsExisted(pteVaddr)) {
                continue;
            }

            virAddr = (uintptr_t)((pdeIdx << 22) | (pteIdx << 12));
            phyAddr = OsUnmapVir2Phy(virAddr);
            if (phyAddr != (uintptr_t)NULL) {
                phyIdx = (U32)((phyAddr - g_usrPhyMemPool.base) / OS_PG_SIZE);
                OsBtmpClear(&g_usrPhyMemPool.btmp, phyIdx);
            }
        }

        /* 释放 PT 页本身（来自内核池） */
        phyAddr = OsUnmapVir2Phy(0xFFC00000 + pdeIdx * 0x1000);
        if (phyAddr != (uintptr_t)NULL) {
            phyIdx = (U32)((phyAddr - g_kernelPhyMemPool.base) / OS_PG_SIZE);
            OsBtmpClear(&g_kernelPhyMemPool.btmp, phyIdx);
        }
        virIdx = (U32)(((0xFFC00000 + pdeIdx * 0x1000) - g_kernelVirMemPool.base) / OS_PG_SIZE);
        OsBtmpClear(&g_kernelVirMemPool.btmp, virIdx);
    }

    /* 切回内核页目录 */
    OsLoadPgd(OS_KERNEL_PGD_BASE);

    /* 释放 PGD 页（来自内核池） */
    phyAddr = OsUnmapVir2Phy(tskCb->pgDir);
    if (phyAddr != (uintptr_t)NULL) {
        phyIdx = (U32)((phyAddr - g_kernelPhyMemPool.base) / OS_PG_SIZE);
        OsBtmpClear(&g_kernelPhyMemPool.btmp, phyIdx);
    }
    virIdx = (U32)((tskCb->pgDir - g_kernelVirMemPool.base) / OS_PG_SIZE);
    OsBtmpClear(&g_kernelVirMemPool.btmp, virIdx);
}

/* 映射用户栈页到进程页表（i386 自映射实现） */
OS_SEC_KERNEL_TEXT void OsProcessMapUsrStackArch(struct OsTaskCb *tskCb, uintptr_t phyAddr)
{
    /* 切到进程页目录（OsMapVir2Phy 依赖自映射，必须 CR3 = 进程 PGD） */
    OsLoadPgd(OsGetPaddrByVaddr(tskCb->pgDir));

    if (!OsMapVir2Phy((uintptr_t)OS_PROCESS_USR_STACK_BASE, phyAddr)) {
        OS_PANIC("OsProcessMapUsrStackArch: map user stack failed\n");
    }

    /* 恢复内核页目录 */
    OsLoadPgd(OS_KERNEL_PGD_BASE);
}
