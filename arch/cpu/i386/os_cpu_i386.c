#include "os_cpu_i386.h"
#include "os_def.h"
#include "os_pgt.h"
#include "os_cpu.h"
#include "os_tss.h"
#include "os_task_external.h"
#include "os_debug_external.h"

OS_SEC_KERNEL_TEXT void OsProcessInitArch(struct OsTaskCb *process)
{   
    uintptr_t pgdir;

    pgdir = OsCreateProcessPgd();
    if (pgdir == NULL) {
        OS_DEBUG_KPRINT("%s\n", "OsProcessInitArch: OsCreateProcessPgd failed");
        while (1) {}
    }

    process->pgDir = pgdir;
}

OS_SEC_KERNEL_TEXT void OsConfigPgdForTskSwitch(struct OsTaskCb *tsk)
{
    uintptr_t pgdPhyAddr; 

    if (tsk->tskType == OS_TASK_PROCESS) {
        /* 获取页目录的物理地址 */
        pgdPhyAddr = OsGetPaddrByVaddr(tsk->pgDir);
        OS_DEBUG_KPRINT("OsConfigPgdForTskSwitch: pgdPhyAddr = 0x%x\n", (U32)pgdPhyAddr);
        OsLoadPgd(pgdPhyAddr);
    } else {
        OsLoadPgd(OS_KERNEL_PGD_BASE);
    }
}

OS_SEC_KERNEL_TEXT void OsConfigTssForTskSwitch(struct OsTaskCb *tsk)
{
    if (tsk->tskType == OS_TASK_PROCESS) {
        OsTssUpdateEsp0((uintptr_t)((U32)tsk->kernelStkTop + 
                        OS_TASK_KERNEL_STACK_SIZE));      
    }
}

OS_SEC_KERNEL_TEXT void OsConfigArchForTskSwitch(struct OsTaskCb *tsk)
{
    OsConfigPgdForTskSwitch(tsk);

    /* 进入进程后，要把内核栈放到tss里存起来，因此要更新tss */
    OsConfigTssForTskSwitch(tsk);
}