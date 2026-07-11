#include "os_process_internal.h"
#include "os_task_external.h"
#include "os_hwi.h"
#include "string.h"
#include "os_sched_external.h"
#include "os_cpu.h"
#include "os_mem_external.h"
#include "os_debug_external.h"
#include "os_reset.h"
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

/* 分配用户栈物理页并标记虚拟位图，页表映射由 arch 层完成 */
static OS_SEC_KERNEL_TEXT void OsProcessInitUsrMem(struct OsTaskCb *tskCb)
{
    uintptr_t phyAddr;
    U32 virIdx;

    phyAddr = OsMemPoolGetFreePgs(&g_usrPhyMemPool, 1);
    if (phyAddr == (uintptr_t)NULL) {
        OS_PANIC("OsProcessInitUsrMem: no free user physical page for stack\n");
    }
    virIdx = (U32)((OS_PROCESS_USR_STACK_BASE - tskCb->usrVirMemPool.base) / OS_PG_SIZE);
    OsBtmpSet(&tskCb->usrVirMemPool.btmp, virIdx);

    /* 架构层：切 CR3 -> 建立页表映射 -> 切回内核 CR3 */
    OsProcessMapUsrStackArch(tskCb, phyAddr);
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

/* 释放进程的全部用户空间资源 */
OS_SEC_KERNEL_TEXT void OsProcessFreeResources(struct OsTaskCb *tskCb)
{
    U32 btmpPgNum, i;
    uintptr_t btmpPage, phyAddr;
    U32 phyIdx, virIdx;

    /* 释放页表、页目录（架构相关：CR3 切换 + 自映射遍历） */
    OsProcessFreeArchResources(tskCb);

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
