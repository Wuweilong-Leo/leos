#include "os_print_external.h"
#include "os_def.h"
#include "os_list_external.h"
#include "os_sched_external.h"
#include "os_task_external.h"
#include "os_debug_external.h"
#include "os_hwi_i386.h"

OS_SEC_KERNEL_TEXT void OsDebugPanicSpin(char *filename, U32 line, const char *func,
                      const char *cond) {
    OsIntLock();
    OS_DEBUG_KPRINT("filename: %s, line: 0x%x, func: %s, cond: %s\n",
                    filename, line, func, cond);
    while (1) {}
}

OS_SEC_KERNEL_TEXT void OsDebugPrintList(struct OsList *list)
{
    struct OsList *tmpNode;

    OS_LIST_FOR_EACH(list, tmpNode) {
        OS_DEBUG_KPRINT("0x%x,0x%x ==> ", (U32)tmpNode->prev, (U32)tmpNode->next);
    }
    OS_DEBUG_KPRINT("list end ");
}

OS_SEC_KERNEL_TEXT void OsDebugPrintRdyList(void)
{
    U32 i;
    struct OsRunQue *rq = OS_RUN_QUE();

    for (i = 0; i < OS_TASK_PRIO_MAX_NUM; i++) {
        OS_DEBUG_KPRINT("prio%x:", i);
        OsDebugPrintList(&rq->rdyList[i]);
    }
}

