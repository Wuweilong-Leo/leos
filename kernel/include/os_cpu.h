#ifndef OS_CPU_H
#define OS_CPU_H
#include "os_def.h"
#include "os_target.h"

/* 通用 CPU 抽象 */
#define OS_PG_SIZE 4096

/* 通用内存映射 API */
extern bool OsMapVir2Phy(uintptr_t virAddr, uintptr_t phyAddr);
extern uintptr_t OsUnmapVir2Phy(uintptr_t virAddr);
extern uintptr_t OsGetPaddrByVaddr(uintptr_t vaddr);
extern void OsLoadPgd(uintptr_t pgdPhyAddr);
extern uintptr_t OsCreateProcessPgd(void);

/* 通用架构接口（由架构层实现） */
struct OsTaskCb;
extern void OsProcessInitArch(struct OsTaskCb *process);
extern void OsProcessMapUsrStackArch(struct OsTaskCb *process, uintptr_t phyAddr);
extern void OsProcessFreeArchResources(struct OsTaskCb *process);
extern void OsConfigArchForTskSwitch(struct OsTaskCb *tsk);

/* 任务上下文初始化（由架构层实现） */
extern void OsSetContext(uintptr_t stkMemBase, size_t stkSize, struct OsTaskCb *tskCb);

/* 进程入口（由架构层实现） */
extern void OsProcessEntry(void (*entry)(void), void *param1, void *param2);

/* 设置 fork 子进程返回值为 0（由架构层实现） */
extern void OsProcessForkSetChildRetval(struct OsTaskCb *child);

/* fork 页表拷贝：遍历父进程用户映射，在子进程 PGD 中逐页重建（由架构层实现）
 * 返回 OS_OK 成功，非零失败（回滚由调用者负责） */
extern U32 OsProcessForkCopyPageTables(struct OsTaskCb *parent, struct OsTaskCb *child,
                                       U8 *tmpBuf);

/* 架构相关定义由下方条件编译引入 */
#if defined(ARCH_i386)
#include "os_cpu_i386.h"
#else
#error "Unsupported architecture. Define ARCH_i386 in os_target.h."
#endif

#endif
