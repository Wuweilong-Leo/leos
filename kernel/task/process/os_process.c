#include "os_process_internal.h"
#include "os_task_external.h"
#include "os_hwi.h"
#include "string.h"
#include "os_sched_external.h"
#include "os_cpu.h"
#include "os_mem_external.h"
#include "os_debug_external.h"
#include "os_reset.h"
#include "os_pgt.h"
#include "os_btmp_external.h"

static OS_SEC_KERNEL_TEXT void OsProcessInitVirMemPool(struct OsTaskCb *process)
{
    uintptr_t btmpBase;
    U32 usrMemBtmpPgNum;

    usrMemBtmpPgNum = OS_BTMP_GET_PG_NUM_BY_MEM_SIZE(OS_USR_VIR_MEM_SIZE);
    btmpBase = OsMemKernelAllocPgs(usrMemBtmpPgNum);
    if (btmpBase == NULL) {
        OS_PANIC("OsMemKernelAllocPgs failed\n");
    }

    OsMemPoolInit(&process->usrVirMemPool, (uintptr_t)OS_USR_MEM_VIR_ADDR_START,
                  OS_USR_VIR_MEM_SIZE, (U8 *)btmpBase);
}

/*
 * 在进程页目录下分配并映射用户栈页。
 * 调用前 CR3 为内核页目录，调用后恢复为内核页目录。
 * 用户堆 FSC 采用惰性初始化：第一次 malloc 时再创建（此时进程已在运行，
 * 缺页处理中 OS_RUNNING_TASK() 能正确返回进程自身）。
 */
static OS_SEC_KERNEL_TEXT void OsProcessInitUsrMem(struct OsTaskCb *tskCb)
{
    uintptr_t phyAddr;
    U32 virIdx;

    /* 切到进程页目录（OsMapVir2Phy 依赖自映射，必须 CR3 = 进程 PGD） */
    OsLoadPgd(OsGetPaddrByVaddr(tskCb->pgDir));

    /* 分配并映射用户栈 */
    phyAddr = OsMemPoolGetFreePgs(&g_usrPhyMemPool, 1);
    if (phyAddr == (uintptr_t)NULL) {
        OS_PANIC("OsProcessInitUsrMem: no free user physical page for stack\n");
    }
    virIdx = (U32)((OS_PROCESS_USR_STACK_BASE - tskCb->usrVirMemPool.base) / OS_PG_SIZE);
    OsBtmpSet(&tskCb->usrVirMemPool.btmp, virIdx);
    if (!OsMapVir2Phy((uintptr_t)OS_PROCESS_USR_STACK_BASE, phyAddr)) {
        OS_PANIC("OsProcessInitUsrMem: map user stack failed\n");
    }

    /* 恢复内核页目录 */
    OsLoadPgd(OS_KERNEL_PGD_BASE);
}

OS_SEC_KERNEL_TEXT U32 OsProcessCreate(struct OsProcessCreateParam *processParam, U32 *pid)
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

    /* 标记为进程 */
    tskCb->tskType = OS_TASK_PROCESS;

    /* 分配用户栈 + 初始化用户堆 FSC（统一切一次 CR3） */
    OsProcessInitUsrMem(tskCb);

    *pid = tskId;

    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsProcessResume(U32 processId)
{
    return OsTaskResume(processId);
}

/* 释放进程的全部用户空间资源：用户物理页、PT 页、PGD 页、虚拟位图页 */
OS_SEC_KERNEL_TEXT void OsProcessFreeResources(struct OsTaskCb *tskCb)
{
    U32 pdeIdx, pteIdx;
    uintptr_t pdeVaddr, pteVaddr, virAddr, phyAddr;
    U32 phyIdx, virIdx;
    U32 btmpPgNum, i;
    uintptr_t btmpPage;

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

    /* 释放虚拟位图页（由 OsMemKernelAllocPgs 分配，来自内核池） */
    btmpPgNum = OS_BTMP_GET_PG_NUM_BY_MEM_SIZE(OS_USR_VIR_MEM_SIZE);
    for (i = 0; i < btmpPgNum; i++) {
        btmpPage = (uintptr_t)tskCb->usrVirMemPool.btmp.base + i * OS_PG_SIZE;
        phyAddr = OsUnmapVir2Phy(btmpPage);
        if (phyAddr != (uintptr_t)NULL) {
            phyIdx = (U32)((phyAddr - g_kernelPhyMemPool.base) / OS_PG_SIZE);
            OsBtmpClear(&g_kernelPhyMemPool.btmp, phyIdx);
        }
        virIdx = (U32)((btmpPage - g_kernelVirMemPool.base) / OS_PG_SIZE);
        OsBtmpClear(&g_kernelVirMemPool.btmp, virIdx);
    }

    tskCb->pgDir = 0;
    tskCb->usrFscCtrl = NULL;
}
