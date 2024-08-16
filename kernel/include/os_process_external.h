#ifndef OS_PROCESS_EXTERNAL_H
#define OS_PROCESS_EXTERNAL_H
#include "os_def.h"
#include "os_task_external.h"

typedef void (*OsProcessEntryFunc) (void *arg1, void *arg2);

#define OS_PROCESS_PARAM_NUM 0x2
#define OS_PROCESS_NAME_MAX_SIZE OS_TASK_NAME_MAX_SIZE

struct OsProcessCreateParam {
    OsProcessEntryFunc entryFunc;
    U32 prio;
    char processName[OS_PROCESS_NAME_MAX_SIZE];
    void *param[OS_PROCESS_PARAM_NUM];
};

extern U32 OsProcessCreate(struct OsProcessCreateParam *param, U32 *processId);
extern U32 OsProcessResume(U32 processId);
#endif