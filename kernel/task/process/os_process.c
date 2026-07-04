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
