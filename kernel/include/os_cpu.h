#ifndef OS_CPU_H
#define OS_CPU_H
#include "os_def.h"

/* 通用 CPU 抽象 */
#define OS_PG_SIZE 4096

/* 通用内存映射 API */
extern void OsMapVir2Phy(uintptr_t virAddr, uintptr_t phyAddr);
extern uintptr_t OsGetPaddrByVaddr(uintptr_t vaddr);
extern void OsLoadPgd(uintptr_t pgdPhyAddr);
extern uintptr_t OsCreateProcessPgd(void);

/* 通用架构接口（由架构层实现） */
struct OsTaskCb;
extern void OsProcessInitArch(struct OsTaskCb *process);
extern void OsConfigArchForTskSwitch(struct OsTaskCb *tsk);

/* 架构相关定义由下方条件编译引入 */
#if defined(ARCH_i386)
#include "os_cpu_i386.h"
#else
#error "Unsupported architecture. Define ARCH_i386."
#endif

#endif
