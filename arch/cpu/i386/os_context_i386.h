#ifndef OS_CONTEXT_I386_H
#define OS_CONTEXT_I386_H
#include "os_def.h"
#include "os_cpu_i386.h"
#include "os_task_external.h"

extern void OsTaskCommonEntry(U32 tskId);

struct OsAllSaveContext {
    U32 saveFlag;
    U32 edi;
    U32 esi;
    U32 ebp;
    U32 espDummy;
    U32 ebx;
    U32 edx;
    U32 ecx;
    U32 eax;
    U32 gs;
    U32 fs;
    U32 es;
    U32 ds;
    U32 errCode;
    uintptr_t eip;
    U32 cs;
    U32 eflags;
    uintptr_t esp;
    U32 ss;
};

struct OsFastSaveContext {
    U32 saveFlag;
    U32 ebp;
    U32 ebx;
    U32 edi;
    U32 esi;
    void *eip; /* 汇编代码ret后，会回到此函数 */
    void *rsvd;    /* 必须保留，充当返回地址压栈空间 */
    U32 tskId;
};

extern void OsSwitch2Process(void);

OS_INLINE void OsSetContext(uintptr_t stkMemBase, U32 stkSize, struct OsTaskCb* tskCb)
{
    U32 stkBot = (U32)stkMemBase + stkSize;
    U32 stkTop = (U32)stkMemBase;
    struct OsFastSaveContext *fastSaveContext;

    stkBot -= sizeof(struct OsAllSaveContext);
    stkBot -= sizeof(struct OsFastSaveContext);

    fastSaveContext = (struct OsFastSaveContext *)stkBot;
    fastSaveContext->saveFlag = OS_FAST_SAVE_FLAG;
    fastSaveContext->eip = OsTaskCommonEntry;
    fastSaveContext->tskId = tskCb->pid;
    tskCb->stkPtr = (uintptr_t)stkBot;
}
#endif