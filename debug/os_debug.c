#include "os_print_external.h"
#include "os_def.h"
#include "os_list_external.h"
#include "os_sched_external.h"
#include "os_task_external.h"

OS_SEC_KERNEL_TEXT void OsDebugPanicSpin(char *filename, int line, const char *func,
                      const char *cond) {
  OsPrintStr("filename: ");
  OsPrintStr(filename);
  OsPrintStr("\n");
  OsPrintStr("line: 0x");
  OsPrintHex(line);
  OsPrintStr("\n");
  OsPrintStr("function: ");
  OsPrintStr((char *)func);
  OsPrintStr("\n");
  OsPrintStr("condition: ");
  OsPrintStr((char *)cond);
  OsPrintStr("\n");
  while (1) {
  }
}

OS_SEC_KERNEL_TEXT void OsDebugPrintList(struct OsList *list)
{
    struct OsList *tmpNode;

    OS_LIST_FOR_EACH(list, tmpNode) {
        kprintf("0x%x,0x%x ==> ", (U32)tmpNode->prev, (U32)tmpNode->next);
    }
    kprintf("list end ");
}

OS_SEC_KERNEL_TEXT void OsDebugPrintRdyList(void)
{
    U32 i;
    struct OsRunQue *rq = OS_RUN_QUE();

    for (i = 0; i < OS_TASK_PRIO_MAX_NUM; i++) {
        kprintf("prio%x:", i);
        OsDebugPrintList(&rq->rdyList[i]);
    }
}

