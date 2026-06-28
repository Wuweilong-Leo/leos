#include "os_symtab_external.h"
#include "os_print_external.h"
#include "string.h"

/*
 * 符号表模块 — 地址→名称 / 名称→地址 查找
 *
 * g_symtab[] 由 gen_symtab.py 从 kernel.map 生成，按地址升序排列
 * OsSymtabLookup  : 二分查找，返回 addr <= 目标 的最近条目
 * OsSymtabAddr2Name: 拼接 "函数名+0x偏移" 格式
 * OsSymtabName2Addr: 精确名称匹配
 * OsSymtabPrefixMatch: 前缀匹配遍历
 */

/* ====== 查找 API ====== */

OS_SEC_KERNEL_TEXT const struct OsSymtabEntry *OsSymtabLookup(uintptr_t addr)
{
    U32 lo, hi, mid;

    if (g_symtabCnt == 0) {
        return (void *)0;
    }

    /* addr 小于表中最小地址 */
    if (addr < g_symtab[0].addr) {
        return (void *)0;
    }

    /* 二分查找：找最大的 i 使得 g_symtab[i].addr <= addr */
    lo = 0;
    hi = g_symtabCnt - 1;

    while (lo < hi) {
        mid = lo + (hi - lo + 1) / 2;
        if (g_symtab[mid].addr <= addr) {
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
    static OS_SEC_KERNEL_BSS char g_buf[48];

    ent = OsSymtabLookup(addr);
    if (ent == (void *)0) {
        /* 未找到：返回裸地址 */
        OsPrintStr("0x");
        OsPrintHex((U32)addr);
        return g_buf;  /* 简化：直接打印，返回占位 */
    }

    offset = addr - ent->addr;

    /* 拼接 "函数名" 或 "函数名+0x偏移" */
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
            return g_symtab[i].addr;
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
