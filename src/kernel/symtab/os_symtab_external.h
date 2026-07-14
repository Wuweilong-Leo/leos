#ifndef OS_SYMTAB_EXTERNAL_H
#define OS_SYMTAB_EXTERNAL_H
#include "os_def.h"

/* ====== 符号表条目 ====== */

struct OsSymtabEntry {
    const void *addr;     /* &func — 链接器解析地址 */
    const char *name;     /* #func — 编译器字符串化 */
};

#define OS_SYMTAB_ENTRY(sym) { (const void *)&(sym), #sym }

/* ====== 符号表数据（由 gen_symtab.py 生成） ====== */

extern const struct OsSymtabEntry g_symtab[];
extern const U32 g_symtabCnt;

/* ====== 查找 API ====== */

/*
 * OsSymtabLookup — 地址二分查找，返回最近的符号条目（addr <= 目标地址）
 * 若 addr 小于表中最小地址，返回 NULL
 */
const struct OsSymtabEntry *OsSymtabLookup(uintptr_t addr);

/*
 * OsSymtabAddr2Name — 地址→"函数名+0x偏移" 字符串
 * 使用内部静态缓冲区，不可重入
 * 未找到时返回 "0xXXXXXXXX" 格式
 */
const char *OsSymtabAddr2Name(uintptr_t addr);

/*
 * OsSymtabName2Addr — 名称→地址（精确匹配）
 * 未找到时返回 0
 */
uintptr_t OsSymtabName2Addr(const char *name);

/*
 * OsSymtabPrefixMatch — 前缀匹配，逐条调用 callback
 * 返回匹配数量
 */
U32 OsSymtabPrefixMatch(const char *prefix,
                        void (*callback)(const struct OsSymtabEntry *));

#endif /* OS_SYMTAB_EXTERNAL_H */
