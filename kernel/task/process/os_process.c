#include "os_process_internal.h"
#include "os_task_external.h"
#include "os_hwi.h"
#include "string.h"
#include "os_sched_external.h"
#include "os_cpu.h"
#include "os_mem_external.h"
#include "os_debug_external.h"

static OS_SEC_KERNEL_TEXT void OsProcessInitVirMemPool(struct OsTaskCb *process)
{
    uintptr_t btmpBase;
    U32 usrMemBtmpPgNum;

    usrMemBtmpPgNum = OS_BTMP_GET_PG_NUM_BY_MEM_SIZE(OS_USR_VIR_MEM_SIZE);
    OS_DEBUG_KPRINT("OsProcessInitVirMemPool: usrMemBtmpPgNum = 0x%x\n", usrMemBtmpPgNum);
    btmpBase = OsMemKernelAllocPgs(usrMemBtmpPgNum);
    if (btmpBase == NULL) {
        OS_DEBUG_KPRINT("%s\n", "OsProcessInitVirMemPool: OsMemKernelAllocPgs failed");
        while (1) {
        }
    }

    OsMemPoolInit(&process->usrVirMemPool, (uintptr_t)OS_USR_MEM_VIR_ADDR_START,
                  OS_USR_VIR_MEM_SIZE, (U8 *)btmpBase);
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

    tskCb->tskType = OS_TASK_PROCESS;
    *pid = tskId;

    OsIntRestore(intSave);
    return OS_OK;
}

OS_SEC_KERNEL_TEXT U32 OsProcessResume(U32 processId)
{
    return OsTaskResume(processId);
}