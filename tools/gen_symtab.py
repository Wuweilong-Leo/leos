#!/usr/bin/env python3
"""
gen_symtab.py — 从 GNU ld -Map 输出中提取内核符号表

用法: python3 gen_symtab.py <kernel.map> <output.c>

解析 kernel.map，提取 Os*/main* 前缀且位于内核空间的符号，
生成包含 OS_SYMTAB_ENTRY() 宏调用的 C 数组。
编译器通过 &(sym) 解析地址，#sym 字符串化函数名。
"""

import re
import sys

# 匹配符号行: 大量空格 + 0x + 16位hex地址 + 空格 + 符号名
SYM_RE = re.compile(r'^\s+0x[0-9a-fA-F]{16}\s+(\S+)$')

# 内核地址范围
KERNEL_ADDR_MIN = 0xC0000000
KERNEL_ADDR_MAX = 0xC0FFFFFF

# 允许的符号前缀
ALLOWED_PREFIXES = ('Os', 'main')

# 排除的模式
EXCLUDE_PATTERNS = re.compile(r'^[$.]')


def parse_map(map_path):
    """解析 kernel.map，返回按地址升序排列的符号名列表"""
    symbols = []
    seen = set()

    with open(map_path, 'r') as f:
        for line in f:
            m = SYM_RE.match(line)
            if not m:
                continue

            name = m.group(1)

            parts = line.strip().split()
            if len(parts) < 2:
                continue

            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue

            if addr < KERNEL_ADDR_MIN or addr > KERNEL_ADDR_MAX:
                continue
            if not any(name.startswith(p) for p in ALLOWED_PREFIXES):
                continue
            if EXCLUDE_PATTERNS.match(name):
                continue
            if name in seen:
                continue
            seen.add(name)

            symbols.append((addr, name))

    symbols.sort(key=lambda x: x[0])
    return symbols


def gen_c_source(symbols):
    """生成 C 源文件内容

    不 include os_symtab_external.h，避免与同名函数声明冲突。
    直接前置声明 struct OsSymtabEntry 和宏 OS_SYMTAB_ENTRY。

    整个符号表用 #ifdef OS_OPTION_SYMTAB 包裹：关闭该特性宏时，
    g_symtab[] / g_symtabCnt 不产出，与 os_symtab.c 的函数体 gate 协同，
    避免数据表里的 OS_SYMTAB_ENTRY(OsSymtabLookup) 反向引用已被裁掉的函数符号。
    os_target.h 经 include 路径提供 OS_OPTION_SYMTAB 定义。
    """
    lines = []
    lines.append('/*')
    lines.append(' * os_symtab_data.c — 自动生成，勿手动编辑')
    lines.append(' * 由 tools/gen_symtab.py 从 kernel.map 生成')
    lines.append(' */')
    lines.append('')
    lines.append('#include "os_def.h"')
    lines.append('#include "os_target.h"')
    lines.append('')
    lines.append('#ifdef OS_OPTION_SYMTAB')
    lines.append('')
    lines.append('struct OsSymtabEntry {')
    lines.append('    const void *addr;')
    lines.append('    const char *name;')
    lines.append('};')
    lines.append('')
    lines.append('#define OS_SYMTAB_ENTRY(sym) { (const void *)&(sym), #sym }')
    lines.append('')

    # extern 声明：让编译器知道这些符号存在
    lines.append('/* extern 声明 */')
    for _, name in symbols:
        lines.append(f'extern const char {name}[];')
    lines.append('')

    lines.append('OS_SEC_KERNEL_DATA const struct OsSymtabEntry g_symtab[] = {')

    for addr, name in symbols:
        lines.append(f'    OS_SYMTAB_ENTRY({name}),')

    lines.append('};')
    lines.append('')
    lines.append(f'OS_SEC_KERNEL_DATA const U32 g_symtabCnt = sizeof(g_symtab) / sizeof(struct OsSymtabEntry);')
    lines.append('')
    lines.append('#endif /* OS_OPTION_SYMTAB */')
    lines.append('')

    return '\n'.join(lines)


def main():
    if len(sys.argv) != 3:
        print(f'用法: {sys.argv[0]} <kernel.map> <output.c>', file=sys.stderr)
        sys.exit(1)

    map_path = sys.argv[1]
    out_path = sys.argv[2]

    symbols = parse_map(map_path)
    if not symbols:
        print('警告: 未提取到任何符号', file=sys.stderr)
        sys.exit(1)

    source = gen_c_source(symbols)

    # 只在内容变化时才写文件，避免触发不必要的重编译
    try:
        with open(out_path, 'r') as f:
            old = f.read()
    except FileNotFoundError:
        old = None

    if old != source:
        with open(out_path, 'w') as f:
            f.write(source)
        print(f'已生成 {out_path}: {len(symbols)} 个符号')
    else:
        print(f'{out_path}: {len(symbols)} 个符号（无变化）')


if __name__ == '__main__':
    main()
