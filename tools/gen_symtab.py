#!/usr/bin/env python3
"""
gen_symtab.py — 从 GNU ld -Map 输出中提取内核符号表

用法: python3 gen_symtab.py <kernel.map> <output.c>

解析 kernel.map，提取 Os*/main* 前缀且位于内核空间(0xC000xxxx)的符号，
生成包含字符串池 + 排序 entry 数组的 C 源文件。
"""

import re
import sys

# 匹配符号行: 大量空格 + 0x + 16位hex地址 + 空格 + 符号名
# 例: "                0x00000000c000d000                main"
SYM_RE = re.compile(r'^\s+0x[0-9a-fA-F]{16}\s+(\S+)$')

# 内核地址范围
KERNEL_ADDR_MIN = 0xC0000000
KERNEL_ADDR_MAX = 0xC0FFFFFF

# 允许的符号前缀（Os* / main*）
ALLOWED_PREFIXES = ('Os', 'main')

# 排除的模式
EXCLUDE_PATTERNS = re.compile(r'^[$.]')  # $d, $x, .hidden 等


def parse_map(map_path):
    """解析 kernel.map，返回 [(addr, name), ...] 列表"""
    symbols = []
    seen = set()

    with open(map_path, 'r') as f:
        for line in f:
            m = SYM_RE.match(line)
            if not m:
                continue

            name = m.group(1)

            # 提取地址：从行中解析
            parts = line.strip().split()
            if len(parts) < 2:
                continue

            try:
                addr = int(parts[0], 16)
            except ValueError:
                continue

            # 过滤：内核地址范围
            if addr < KERNEL_ADDR_MIN or addr > KERNEL_ADDR_MAX:
                continue

            # 过滤：允许的前缀
            if not any(name.startswith(p) for p in ALLOWED_PREFIXES):
                continue

            # 过滤：排除链接器辅助符号
            if EXCLUDE_PATTERNS.match(name):
                continue

            # 去重（同名符号取第一个出现的）
            if name in seen:
                continue
            seen.add(name)

            symbols.append((addr, name))

    # 按地址升序排序
    symbols.sort(key=lambda x: x[0])
    return symbols


def gen_c_source(symbols):
    """生成 C 源文件内容"""
    lines = []
    lines.append('/*')
    lines.append(' * os_symtab_data.c — 自动生成，勿手动编辑')
    lines.append(' * 由 tools/gen_symtab.py 从 kernel.map 生成')
    lines.append(' */')
    lines.append('')
    lines.append('#include "os_symtab_external.h"')
    lines.append('#include "os_def.h"')
    lines.append('')

    # 字符串池
    lines.append('OS_SEC_KERNEL_DATA const char g_symtab_str[] =')
    offset_map = {}
    current_offset = 0

    for addr, name in symbols:
        offset_map[name] = current_offset
        lines.append(f'    "{name}\\0"')
        current_offset += len(name) + 1  # +1 for \0

    lines.append(';')
    lines.append('')

    # entry 数组
    lines.append('OS_SEC_KERNEL_DATA const struct OsSymtabEntry g_symtab[] = {')

    for addr, name in symbols:
        lines.append(f'    {{0x{addr:08X}, &g_symtab_str[{offset_map[name]}]}},  /* {name} */')

    lines.append('};')
    lines.append('')

    # count
    lines.append(f'OS_SEC_KERNEL_DATA const U32 g_symtabCnt = {len(symbols)};')
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

    with open(out_path, 'w') as f:
        f.write(source)

    print(f'已生成 {out_path}: {len(symbols)} 个符号')


if __name__ == '__main__':
    main()
