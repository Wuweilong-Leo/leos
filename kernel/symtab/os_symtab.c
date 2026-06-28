#include "os_symtab_external.h"
#include "os_print_external.h"
#include "string.h"

/*
 * 符号表模块 — 地址→名称 / 名称→地址 查找
 *
 * g_symtab[] 由 gen_symtab.py 从 kernel.map 生成，按地址升序排列
 * 每条记录由 OS_SYMTAB_ENTRY(sym) 展开: { &sym, "sym" }
 * OsSymtabLookup  : 二分查找，返回 addr <= 目标 的最近条目
 * OsSymtabAddr2Name: 拼接 "函数名+0x偏移" 格式
 * OsSymtabName2Addr: 精确名称匹配
 * OsSymtabPrefixMatch: 前缀匹配遍历
 */

/* ====== 内联辅助 ====== */

OS_SEC_KERNEL_TEXT OS_INLINE uintptr_t OsSymtabEntryAddr(U32 idx)
{
    return (uintptr_t)g_symtab[idx].addr;
}

/* ====== 查找 API ====== */

OS_SEC_KERNEL_TEXT const struct OsSymtabEntry *OsSymtabLookup(uintptr_t addr)
{
    U32 lo, hi, mid;

    if (g_symtabCnt == 0) {
        return (void *)0;
    }

    /* addr 小于表中最小地址 */
    if (addr < OsSymtabEntryAddr(0)) {
        return (void *)0;
    }

    /* 二分查找：找最大的 i 使得 g_symtab[i].addr <= addr */
    lo = 0;
    hi = g_symtabCnt - 1;

    while (lo < hi) {
        mid = lo + (hi - lo + 1) / 2;
        if (OsSymtabEntryAddr(mid) <= addr) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    return &g_symtab[lo];
}

OS_SEC_KERNEL_TEXT const char *OsSymtabAddr2Name(uintptr_t addr)
{
    const struct OsSymtabEntry *ent;
    uintptr_t offset;

    ent = OsSymtabLookup(addr);
    if (ent == (void *)0) {
        kprintf("0x%x", (U32)addr);
        return (void *)0;
    }

    offset = addr - (uintptr_t)ent->addr;

    kprintf("%s", ent->name);
    if (offset != 0) {
        kprintf("+0x%x", (U32)offset);
    }

    return ent->name;
}

OS_SEC_KERNEL_TEXT uintptr_t OsSymtabName2Addr(const char *name)
{
    U32 i;

    for (i = 0; i < g_symtabCnt; i++) {
        if (strcmp(name, g_symtab[i].name) == 0) {
            return (uintptr_t)g_symtab[i].addr;
        }
    }

    return 0;
}

OS_SEC_KERNEL_TEXT U32 OsSymtabPrefixMatch(const char *prefix,
                                           void (*callback)(const struct OsSymtabEntry *))
{
    U32 i;
    U32 count = 0;
    U32 prefixLen;
    U32 j;
    bool match;

    prefixLen = (U32)strlen(prefix);

    for (i = 0; i < g_symtabCnt; i++) {
        match = TRUE;
        for (j = 0; j < prefixLen; j++) {
            if (g_symtab[i].name[j] != prefix[j]) {
                match = FALSE;
                break;
            }
        }
        if (match) {
            callback(&g_symtab[i]);
            count++;
        }
    }

    return count;
}
