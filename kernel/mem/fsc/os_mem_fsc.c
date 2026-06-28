#include "os_mem_fsc_internal.h"
#include "os_base_external.h"
#include "os_hwi.h"
#include "os_debug_external.h"

OS_INLINE struct OsMemFscHead *OsMemFscGetFreeList(struct OsMemFscCtrl *ctrl, U32 idx)
{
    return &ctrl->freeList[idx];
}

OS_INLINE U32 OsMemFscSize2Idx(size_t size)
{
    return 31 - OsGetLmb(size);
}

OS_INLINE U32 OsMemFscIdx2Bit(U32 idx)
{
    return 0x80000000U >> idx;
}

OS_SEC_KERNEL_TEXT void OsMemFscFreeListInsertBlk(struct OsMemFscCtrl *ctrl,
                                                  struct OsMemFscHead *blk)
{
    U32 idx = OsMemFscSize2Idx(blk->size);
    struct OsMemFscHead *freeList = OsMemFscGetFreeList(ctrl, idx);

    blk->next = freeList->next;
    blk->prev = freeList;
    freeList->next->prev = blk;
    freeList->next = blk;

    ctrl->btmp |= OsMemFscIdx2Bit(idx);
}

OS_SEC_KERNEL_TEXT void OsMemFscFreeListRemoveBlk(struct OsMemFscHead *blk)
{
    blk->prev->next = blk->next;
    blk->next->prev = blk->prev;
}

OS_INLINE bool OsMemFscFreeListIsEmpty(const struct OsMemFscHead *freeList)
{
    return freeList->next == freeList;
}

OS_SEC_KERNEL_TEXT struct OsMemFscCtrl *OsMemFscInitPt(uintptr_t addr, size_t size)
{
    struct OsMemFscCtrl *ptCtrl;
    size_t ptSize;
    struct OsMemFscHead *blk;
    U32 i;
    struct OsMemFscHead *freeList;
    size_t blkSize;

    ptCtrl = (struct OsMemFscCtrl *)OS_ROUND_UP(addr, 4);
    ptSize = (size_t)OS_ROUND_DOWN(size, 4);
    blk = (struct OsMemFscHead *)(ptCtrl + 1);
    blkSize =
        (uintptr_t)ptCtrl + ptSize - (uintptr_t)blk - OS_MEM_FSC_HEAD_SIZE; // 此处再预留个尾巴占位

    for (i = 0; i < OS_MEM_FSC_SIZE_NUM; i++) {
        freeList = OsMemFscGetFreeList(ptCtrl, i);
        freeList->next = freeList;
        freeList->prev = freeList;
    }

    ptCtrl->btmp = 1; // 这个BIT表示2的N次大小，不会挂内存块，但是置1
    blk->preSize = 0;
    blk->size = blkSize;
    OsMemFscFreeListInsertBlk(ptCtrl, blk);
    ptCtrl->totalSize = blkSize;
    ptCtrl->freeSize = blkSize;
    return ptCtrl;
}

OS_SEC_KERNEL_TEXT struct OsMemFscHead *OsMemFscFuzzySearch(struct OsMemFscCtrl *ptCtrl,
                                                            size_t size)
{
    // 扫描所有比当前需要的内存大的链表
    U32 idx = OsMemFscSize2Idx(size - 1) + 1; // 防止正好是2的N次幂
    U32 *btmp = &ptCtrl->btmp;
    struct OsMemFscHead *freeList;

    while (1) {
        if (idx >= 32) {
            return NULL;
        }
        idx = OsGetLmb(((*btmp) << idx) >> idx); // 先过滤掉小的内存然后从小链表开始找
        if (idx == OS_MEM_FSC_LAST_IDX) {
            return NULL;
        }

        freeList = OsMemFscGetFreeList(ptCtrl, idx);
        if (OsMemFscFreeListIsEmpty(freeList)) {
            // 是空的就再找
            *btmp &= ~OsMemFscIdx2Bit(idx);
            continue;
        }

        return freeList->next;
    }
}

OS_SEC_KERNEL_TEXT void *OsMemFscExactSearch(struct OsMemFscCtrl *ptCtrl, size_t allocSize,
                                             size_t alignSize, U32 align)
{
    U32 idx = OsMemFscSize2Idx(allocSize - 1);
    struct OsMemFscHead *curBlk;
    struct OsMemFscHead *freeList = OsMemFscGetFreeList(ptCtrl, idx);

    curBlk = freeList->next;
    while (curBlk != freeList) {
        if ((curBlk->size >= allocSize) ||
            ((OS_ROUND_UP((uintptr_t)curBlk + OS_MEM_FSC_HEAD_SIZE, align) + alignSize +
              OS_MEM_FSC_TAIL_MAGIC_SIZE - (uintptr_t)curBlk) <= curBlk->size)) {
            // 不满足allocSize不一定不可用，allocSize本身就偏大，需要重新按照地址算一遍
            return curBlk;
        }

        curBlk = curBlk->next;
    }

    return NULL;
}

OS_SEC_KERNEL_TEXT bool OsMemFscTrySplitRightBlk(struct OsMemFscCtrl *ctrl, uintptr_t rightBlk,
                                                 size_t rightBlkSize)
{
    struct OsMemFscHead *rightBlkHead = (struct OsMemFscHead *)rightBlk;

    if (rightBlkSize >= OS_MEM_FSC_MIN_SIZE) {
        // 如果剩余的内存可以成一个独立的块
        rightBlkHead->size = rightBlkSize;
        rightBlkHead->ctrl = NULL; // 是free状态的，置0
        rightBlkHead->preSize = 0; // 上一块必是使用块，置0
        OsMemFscFreeListInsertBlk(ctrl, rightBlkHead);
        return TRUE;
    }

    return FALSE;
}

OS_SEC_KERNEL_TEXT bool OsMemFscTrySplitLeftBlk(struct OsMemFscCtrl *ctrl, uintptr_t leftBlk,
                                                size_t leftBlkSize, uintptr_t curBlk)
{
    struct OsMemFscHead *leftBlkHead = (struct OsMemFscHead *)leftBlk;
    struct OsMemFscHead *curBlkHead = (struct OsMemFscHead *)curBlk;

    if (leftBlkSize >= OS_MEM_FSC_MIN_SIZE) {
        // 如果剩余的内存可以成一个独立的块
        leftBlkHead->size = leftBlkSize;
        leftBlkHead->ctrl = NULL; // 是free状态的，置0
        // 左侧块头本身就是原先空闲块的头了，preSize继承之前的
        // 大小不满足了，链表要重新挂
        OsMemFscFreeListInsertBlk(ctrl, leftBlkHead);

        // 因为左块是空闲块，更新本块的preSize
        curBlkHead->preSize = leftBlkSize;
        return TRUE;
    }

    return FALSE;
}

OS_SEC_KERNEL_TEXT void OsMemFscSetTailMagic(uintptr_t addr, size_t size)
{
    *(U32 *)(addr + size - 4) = OS_MEM_FSC_TAIL_MAGIC;
}

OS_SEC_KERNEL_TEXT void OsMemFscSetOffset(uintptr_t head, uintptr_t usrAddr)
{
    *(U32 *)(usrAddr - 4) = usrAddr - head;
}

OS_SEC_KERNEL_TEXT void *OsMemFscAlloc(struct OsMemFscCtrl *ptCtrl, size_t size, U32 align)
{
    size_t allocSize;
    struct OsMemFscHead *blk;
    size_t alignSize;
    uintptr_t nextBlk;
    uintptr_t realBlk;
    uintptr_t rightBlk;
    size_t realSize;
    uintptr_t usrAddr;
    size_t leftBlkSize = 0;
    size_t rightBlkSize;
    enum OsIntStatus intSave;

    alignSize = OS_ROUND_UP(size, 4); // 保证所有操作都4字节对齐
    // 已经按照4字节对齐，如果对齐的话最大补齐也只可能是align - 4，这个大小算出来是偏大的
    allocSize = alignSize + (align - 4) + OS_MEM_FSC_HEAD_SIZE + OS_MEM_FSC_TAIL_MAGIC_SIZE;

    intSave = OsIntLock();
    blk = OsMemFscFuzzySearch(ptCtrl, allocSize);
    if (blk == NULL) {
        blk = OsMemFscExactSearch(ptCtrl, allocSize, alignSize, align);
        if (blk == NULL) {
            OS_LOG_ERROR("no suitable block, size=%u align=%u\n", (size_t)size, align);
            OsIntRestore(intSave);
            return NULL;
        }
    }

    OsMemFscFreeListRemoveBlk(blk); // 先把此块摘出去
    nextBlk = (uintptr_t)blk + blk->size;
    // 从后往前切割，获得能保证对齐的首地址
    realBlk = OS_ROUND_DOWN(nextBlk - OS_MEM_FSC_TAIL_MAGIC_SIZE - alignSize, align) -
              OS_MEM_FSC_HEAD_SIZE;
    // 用这个首地址获取到尾地址，可能会存在尾部块
    rightBlk = realBlk + OS_MEM_FSC_HEAD_SIZE + alignSize + OS_MEM_FSC_TAIL_MAGIC_SIZE;
    realSize = OS_MEM_FSC_HEAD_SIZE + alignSize + OS_MEM_FSC_TAIL_MAGIC_SIZE;

    // 尝试切割右块，切不了，大小要算入本块
    rightBlkSize = nextBlk - rightBlk;
    if (!OsMemFscTrySplitRightBlk(ptCtrl, rightBlk, rightBlkSize)) {
        realSize += rightBlkSize;
        ((struct OsMemFscHead *)nextBlk)->preSize = 0;
    } else {
        ((struct OsMemFscHead *)nextBlk)->preSize = rightBlkSize;
    }

    // 尝试切割左块，切不了，大小要算入本块
    if (!OsMemFscTrySplitLeftBlk(ptCtrl, (uintptr_t)blk, realBlk - (uintptr_t)blk, realBlk)) {
        leftBlkSize = realBlk - (uintptr_t)blk;
        realSize += leftBlkSize;
        realBlk = (uintptr_t)blk;
    }

    ((struct OsMemFscHead *)realBlk)->size = realSize;
    ((struct OsMemFscHead *)realBlk)->ctrl = ptCtrl;
    OsMemFscSetTailMagic(realBlk, realSize);
    usrAddr = realBlk + OS_MEM_FSC_HEAD_SIZE + leftBlkSize;
    OsMemFscSetOffset(realBlk, usrAddr);
    ptCtrl->freeSize -= realSize;

    OsIntRestore(intSave);
    return (void *)usrAddr;
}

OS_SEC_KERNEL_TEXT struct OsMemFscHead *OsMemFscGetHead(uintptr_t addr)
{
    return (struct OsMemFscHead *)(addr - (uintptr_t)(*(U32 *)(addr - 4)));
}

OS_SEC_KERNEL_TEXT void OsMemFscTryMergeRight(struct OsMemFscCtrl *ctrl,
                                              struct OsMemFscHead *curBlk)
{
    struct OsMemFscHead *rightBlk = (struct OsMemFscHead *)((uintptr_t)curBlk + curBlk->size);

    if (rightBlk->ctrl == NULL) {
        // 下一块是free块，可以合并
        OsMemFscFreeListRemoveBlk(rightBlk);
        curBlk->size += rightBlk->size;
    }
}

OS_SEC_KERNEL_TEXT bool OsMemFscTryMergeLeft(struct OsMemFscCtrl *ctrl, struct OsMemFscHead *curBlk,
                                             struct OsMemFscHead **mergedBlk)
{
    struct OsMemFscHead *leftBlk;

    if (curBlk->preSize != 0) {
        // 上一块是空闲的
        leftBlk = (struct OsMemFscHead *)((uintptr_t)curBlk - curBlk->preSize);
        OsMemFscFreeListRemoveBlk(leftBlk);
        leftBlk->size += curBlk->size;
        if (mergedBlk != NULL) {
            *mergedBlk = leftBlk;
        }
        return TRUE;
    }

    return FALSE;
}

OS_SEC_KERNEL_TEXT void OsMemFscFree(void *addr)
{
    struct OsMemFscHead *memHead;
    struct OsMemFscCtrl *ctrl;
    size_t size;
    enum OsIntStatus intSave = OsIntLock();

    memHead = OsMemFscGetHead((uintptr_t)addr);
    ctrl = memHead->ctrl;
    size = memHead->size;

    OsMemFscTryMergeRight(ctrl, memHead);

    struct OsMemFscHead *mergedLeft = NULL;
    if (OsMemFscTryMergeLeft(ctrl, memHead, &mergedLeft)) {
        memHead = mergedLeft;
    }
    ((struct OsMemFscHead *)((uintptr_t)memHead + memHead->size))->preSize = memHead->size;
    OsMemFscFreeListInsertBlk(ctrl, memHead);
    /* 用入口处捕获的 ctrl：左合并后 memHead 指向左邻空闲块，其 ctrl==NULL，
       直接用 memHead->ctrl 会解引用 NULL 把账记到地址 0，导致 freeSize 永不增加 */
    ctrl->freeSize += size;
    memHead->ctrl = NULL;
    OsIntRestore(intSave);
    return;
}