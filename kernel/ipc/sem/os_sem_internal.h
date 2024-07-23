#ifndef OS_SEM_INTERNAL_H
#define OS_SEM_INTERNAL_H
#include "os_sem_external.h"
#include "os_sys.h"

#define OS_SEM_MAX_NUM 0x10
#define OS_SEM_GET_CB(semId) (&g_semCbArray[(semId)])

#define OS_SEM_PEND_TSK_ALREADY_HOLD_SEM OS_BUILD_ERR_CODE(OS_MID_SEM, 0x0);
#define OS_SEM_POST_TSK_NOT_HOLD_SEM OS_BUILD_ERR_CODE(OS_MID_SEM, 0x1);
#endif