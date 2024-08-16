#include "os_def.h"
#include "os_process_external.h"
#include "os_cpu_i386.h"

#define OS_PROCESS_USR_STACK_BASE (0xC0000000 - OS_PG_SIZE)