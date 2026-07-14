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
    tskCb->pgShareMaster = tskCb;   /* 独立进程：指向自己 */
    tskCb->pgDirRefCnt = 1;         /* 初始引用计数=1 */
    tskCb->detached = FALSE;        /* 独立进程默认 joinable */

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

/* ====== fork 实现 ====== */

/* fork 临时缓冲（静态 4KB，用于 CR3 交替时暂存页面内容）
 * 单核关中断下不会并发使用 */
static OS_SEC_KERNEL_BSS U8 g_forkTmpBuf[OS_PG_SIZE];

/* fork 回滚：释放子进程已分配的资源 */
static OS_SEC_KERNEL_TEXT void OsProcessForkRollback(struct OsTaskCb *child, uintptr_t childStk,
                                                      bool pgdCreated, bool btmpCreated,
                                                      uintptr_t newBtmpBase, U32 btmpPgNum)
{
    if (pgdCreated && child->pgDir != 0) {
        OsProcessFreeArchResources(child);
        child->pgDir = 0;
    }
    if (btmpCreated && newBtmpBase != 0) {
        U32 i;
        for (i = 0; i < btmpPgNum; i++) {
            uintptr_t btmpPage = newBtmpBase + i * OS_PG_SIZE;
            uintptr_t phyAddr = OsUnmapVir2Phy(btmpPage);
            if (phyAddr != (uintptr_t)NULL) {
                U32 phyIdx = (U32)((phyAddr - g_kernelPhyMemPool.base) / OS_PG_SIZE);
                OsBtmpClear(&g_kernelPhyMemPool.btmp, phyIdx);
            }
            {
                U32 virIdx = (U32)((btmpPage - g_kernelVirMemPool.base) / OS_PG_SIZE);
                OsBtmpClear(&g_kernelVirMemPool.btmp, virIdx);
            }
        }
    }
    OsMemKernelFree((void *)childStk);
    OsTaskReleaseFreeCb(child);
}

OS_SEC_KERNEL_TEXT U32 OsProcessFork(void)
{
    struct OsTaskCb *parent = OS_RUNNING_TASK();
    struct OsTaskCb *child;
    U32 childPid;
    uintptr_t childStk;
    enum OsIntStatus intSave;
    U32 btmpPgNum;
    uintptr_t newBtmpBase = 0;
    bool pgdCreated = FALSE;
    bool btmpCreated = FALSE;

    intSave = OsIntLock();

    /* === 步骤 1: 分配子进程 TCB === */
    child = OsTaskGetFreeCb();
    if (child == NULL) {
        OsIntRestore(intSave);
        return (U32)-1;
    }
    childPid = child->pid;

    /* === 步骤 2: 分配内核栈 === */
    childStk = (uintptr_t)OsMemKernelAlloc(OS_TASK_KERNEL_STACK_SIZE, 16);
    if (childStk == 0) {
        OsTaskReleaseFreeCb(child);
        OsIntRestore(intSave);
        return (U32)-1;
    }

    /* === 步骤 3: memcpy 内核栈 === */
    memcpy((void *)childStk, (void *)parent->kernelStkTop, OS_TASK_KERNEL_STACK_SIZE);
    child->kernelStkTop = childStk;

    /* === 步骤 4: 设置 stkPtr 偏移 === */
    child->stkPtr = child->kernelStkTop - parent->kernelStkTop + parent->stkPtr;

    /* === 步骤 5: 拷贝 TCB 基本字段 === */
    child->status = OS_TASK_STATUS_USED | OS_TASK_STATUS_SUSPENDED;
    child->prio = parent->prio;
    child->oriPrio = parent->oriPrio;
    child->entry = parent->entry;
    memcpy(child->arg, parent->arg, sizeof(parent->arg));
    child->timeSliceTicks = parent->timeSliceTicks;
    child->expiredTick = 0;
    strcpy(child->name, parent->name);
    child->eventMsk = 0;
    child->curEvent = 0;
    child->tskType = OS_TASK_PROCESS;
    child->pgShareMaster = child;    /* fork 子进程有独立地址空间 */
    child->pgDirRefCnt = 1;
    child->detached = FALSE;         /* fork 子进程默认 joinable */
    child->holdSemList.next = &child->holdSemList;
    child->holdSemList.prev = &child->holdSemList;
    child->msgList.next = &child->msgList;
    child->msgList.prev = &child->msgList;
    child->parentPid = parent->pid;
    child->exitCode = 0;
    child->waitPid = 0;

    /* === 步骤 6: 分配子进程 PGD === */
    child->pgDir = OsCreateProcessPgd();
    if (child->pgDir == 0) {
        OsMemKernelFree((void *)childStk);
        OsTaskReleaseFreeCb(child);
        OsIntRestore(intSave);
        return (U32)-1;
    }
    pgdCreated = TRUE;

    /* === 步骤 7: 深拷贝 usrVirMemPool === */
    btmpPgNum = OS_BTMP_GET_PG_NUM_BY_MEM_SIZE(OS_USR_VIR_MEM_SIZE);
    newBtmpBase = OsMemKernelAllocPgs(btmpPgNum);
    if (newBtmpBase == 0) {
        OsProcessForkRollback(child, childStk, pgdCreated, FALSE, 0, 0);
        OsIntRestore(intSave);
        return (U32)-1;
    }
    btmpCreated = TRUE;

    /* 拷贝位图内容 */
    for (U32 bi = 0; bi < btmpPgNum; bi++) {
        memcpy((void *)(newBtmpBase + bi * OS_PG_SIZE),
               (void *)((uintptr_t)parent->usrVirMemPool.btmp.base + bi * OS_PG_SIZE),
               OS_PG_SIZE);
    }

    /* 拷贝 pool 结构体，替换位图指针，重新初始化链表 */
    memcpy(&child->usrVirMemPool, &parent->usrVirMemPool, sizeof(struct OsMemPool));
    child->usrVirMemPool.btmp.base = (U8 *)newBtmpBase;
    OsListInit(&child->usrVirMemPool.memCtrlList);

    /* === 步骤 8: 子进程 FSC 设为 NULL（惰性初始化） === */
    child->usrFscCtrl = NULL;

    /* === 步骤 9-10: CR3 交替逐页拷贝用户映射（架构层实现） === */
    {
        U32 copyRet = OsProcessForkCopyPageTables(parent, child, g_forkTmpBuf);
        if (copyRet != OS_OK) {
            OsProcessForkRollback(child, childStk, pgdCreated, btmpCreated,
                                  newBtmpBase, btmpPgNum);
            OsIntRestore(intSave);
            return (U32)-1;
        }
    }

    /* === 步骤 11: 子进程 fork 返回 0 === */
    OsProcessForkSetChildRetval(child);

    /* === 步骤 12: 将子进程加入就绪队列 === */
    OsTaskResume(childPid);

    OsIntRestore(intSave);
    return childPid;
}
