#ifndef OS_SEM_INTERNAL_H
#define OS_SEM_INTERNAL_H
#include "os_sem_external.h"
#include "os_sys.h"

#define OS_SEM_MAX_NUM       0x10
#define OS_SEM_GET_CB(semId) (&g_semCbArray[(semId)])

#endif