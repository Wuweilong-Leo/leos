#ifndef OS_MEM_FSC_INTERNAL_H
#define OS_MEM_FSC_INTERNAL_H
#include "os_def.h"
#include "os_btmp_external.h"

#define OS_MEM_FSC_SIZE_NUM        32
#define OS_MEM_FSC_LAST_IDX        (OS_MEM_FSC_SIZE_NUM - 1)
#define OS_MEM_FSC_TAIL_MAGIC      0xCDCDCDCDU
#define OS_MEM_FSC_TAIL_MAGIC_SIZE sizeof(OS_MEM_FSC_TAIL_MAGIC)
#define OS_MEM_FSC_HEAD_SIZE       sizeof(struct OsMemFscHead)
#define OS_MEM_FSC_MIN_SIZE        (OS_MEM_FSC_HEAD_SIZE + 4 + OS_MEM_FSC_TAIL_MAGIC_SIZE)
struct OsMemFscHead
{
    struct OsMemFscCtrl *ctrl;
    struct OsMemFscHead *next;
    size_t size;
    size_t preSize; // 若前面相邻的块空闲，则此字段记录前面空闲块大小，否则为0
    union
    {
        U32 offset;                // 用户地址到地址头的偏移，使用状态下有效
        struct OsMemFscHead *prev; // 挂载上一块内存块，空闲状态有效
    };
};

struct OsMemFscCtrl
{
    U32 btmp;
    struct OsMemFscHead freeList[OS_MEM_FSC_SIZE_NUM];
    size_t totalSize;
    size_t freeSize;
    void *memCtrl;
};

extern struct OsMemFscCtrl *OsMemFscInitPt(uintptr_t addr, size_t size);
extern void *OsMemFscAlloc(struct OsMemFscCtrl *ctrl, size_t size, U32 align);
extern void OsMemFscFree(void *addr);
extern struct OsMemFscHead *OsMemFscGetHead(uintptr_t addr);
#endif