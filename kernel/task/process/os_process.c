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
 * 不切换 CR3——直接通过进程页目录的虚拟地址操作页表。
 */
static OS_SEC_KERNEL_TEXT uintptr_t OsProcessAllocUsrStack(struct OsTaskCb *tskCb)
{
    uintptr_t pgdVaddr = tskCb->pgDir;
    uintptr_t pdeVaddr;
    uintptr_t pteVaddr;
    uintptr_t phyAddr;
    U32 pdeVal;
    U32 pteIdx;

    /* 1. 从用户物理池分配 1 页 */
    phyAddr = OsMemPoolGetFreePgs(&g_usrPhyMemPool, 1);
    if (phyAddr == (uintptr_t)NULL) {
        OS_PANIC("OsProcessAllocUsrStack: no free user physical page\n");
    }

    /* 2. 在进程虚拟位图中标记已占用 */
    {
        U32 virIdx = (U32)((OS_PROCESS_USR_STACK_BASE - tskCb->usrVirMemPool.base) / OS_PG_SIZE);
        OsBtmpSet(&tskCb->usrVirMemPool.btmp, virIdx);
    }

    /* 3. 在进程页目录中映射该页 */
    /* 切到进程页目录来做映射（OsMapVir2Phy 依赖当前 CR3 的自映射） */
    OsLoadPgd(OsGetPaddrByVaddr(pgdVaddr));

    if (!OsMapVir2Phy((uintptr_t)OS_PROCESS_USR_STACK_BASE, phyAddr)) {
        OS_PANIC("OsProcessAllocUsrStack: OsMapVir2Phy failed\n");
    }

    OsLoadPgd(OS_KERNEL_PGD_BASE);

    return (uintptr_t)OS_PROCESS_USR_STACK_BASE;
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

    /* 在进程页目录下分配用户栈（切 CR3） */
    OsProcessAllocUsrStack(tskCb);

    tskCb->tskType = OS_TASK_PROCESS;
    *pid = tskId;

    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsProcessResume(U32 processId)
{
    return OsTaskResume(processId);
}
