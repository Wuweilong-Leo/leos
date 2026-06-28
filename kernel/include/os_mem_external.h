#ifndef OS_MEM_EXTERNAL_H
#define OS_MEM_EXTERNAL_H
#include "os_def.h"
#include "os_sys.h"
#include "os_btmp_external.h"
#include "os_mem_fsc_internal.h"
#include "os_list_external.h"

/* 内存模块错误码 */
#define OS_MEM_MAP_FAIL  OS_BUILD_ERR_CODE(OS_MID_MEM, 0x0) /* 虚实映射失败(如物理页耗尽) */

struct OsMemCtrl {
    struct OsList listNode;
    uintptr_t memBase;
    size_t memSize;
    struct OsFscMemCtrl *fscCtrl;
};

struct OsMemPool {
    struct OsBtmp btmp;
    uintptr_t base;
    size_t size;
    struct OsList memCtrlList;
};

#define OS_KERNEL_MEM_VIR_ADDR_START 0xc0000000
#define OS_USR_MEM_VIR_ADDR_START    0x8048000
/* 用户最多申请1M */
#define OS_USR_VIR_MEM_SIZE         (OS_KERNEL_MEM_VIR_ADDR_START - OS_USR_MEM_VIR_ADDR_START)
#define OS_KERNEL_VIR_HEAP_MEM_BASE 0xC0200000
#define OS_KERNEL_VIR_HEAP_MEM_SIZE (4 * 1024 * 1024)

extern U32 OsMemConfigInit(void);
extern uintptr_t OsMemPoolGetFreePgs(struct OsMemPool *pool, size_t cnt);
extern uintptr_t OsMemKernelAllocPgs(size_t cnt);
extern uintptr_t OsMemUsrAllocPgs(size_t cnt);
extern uintptr_t OsMemUsrAllocPgByAddr(uintptr_t virAddr);
extern uintptr_t OsMemKernelAllocPgByAddr(uintptr_t virAddr);
extern void OsMemPoolInit(struct OsMemPool *memPool, uintptr_t memBase, size_t memSize, U8 *btmpBase);

extern struct OsMemPool g_kernelPhyMemPool;
extern struct OsMemPool g_usrPhyMemPool;
extern struct OsMemPool g_kernelVirMemPool;
void *OsMemKernelAlloc(size_t size, U32 align);
void OsMemKernelFree(void *addr);

#endif