# =============================================================================
# 对象库 + 手工 ld 链接 + 符号表两阶段生成 + dis 目标
# 链接命令与 makefile 对齐;dis 命令与 makefile 一致。
#
# 符号表两阶段生成流程(与 makefile 对齐,一次构建即出真表):
#   stage1: 用占位 os_symtab_data.o 链接 → 提取 kernel.map
#           (Os* 函数在 .KERNEL_TEXT,g_symtab 在 .KERNEL_DATA;占位符大小变化
#            只影响 .data 布局,不影响 .text 中 Os* 地址 → stage1 map 地址准确)
#   生成 : gen_symtab 从 map 生成真 os_symtab_data.c
#   stage2: 重编真 data.o → 重新链接 → 得含真符号表的 elf
#   g_symtab[] 存的是 &(symbol),由 stage2 链接器解析,不依赖 map 数值,双重保障。
#
# os_symtab_data.c 不进对象库,在 custom_command 里单独 gcc 编译:
# 避免占位符 data.o 与 stage2 真 data.o 同时进链接导致 g_symtab 重复定义。
# =============================================================================

set(LEOS_OUTPUT_DIR  ${CMAKE_BINARY_DIR})
set(LEOS_ELF         ${LEOS_OUTPUT_DIR}/os_kernel.elf)
set(LEOS_MAP         ${LEOS_OUTPUT_DIR}/kernel.map)
set(LEOS_LINK_SCRIPT ${CMAKE_SOURCE_DIR}/src/ld_script/os_ld.S)
set(LEOS_SYMTAB_SRC  ${CMAKE_SOURCE_DIR}/src/kernel/symtab/os_symtab_data.c)
set(LEOS_SYMTAB_SCRIPT ${CMAKE_SOURCE_DIR}/tools/gen_symtab.py)
set(LEOS_SYMTAB_OBJ  ${LEOS_OUTPUT_DIR}/os_symtab_data.o)
set(LEOS_ELF_STAGE1  ${LEOS_ELF}.stage1)

# 从源列表移除 os_symtab_data.c —— 单独在两阶段链接里编译,不进对象库
list(REMOVE_ITEM LEOS_C_SRCS ${LEOS_SYMTAB_SRC})

# 编译 symtab data.o 用的 -I 参数(对象库经 include_directories 自动带,手动 gcc 不带)
set(LEOS_INCS "")
foreach(dir ${LEOS_INC_DIRS})
    list(APPEND LEOS_INCS "-I${dir}")
endforeach()

# CMAKE_C_FLAGS 是空格分隔的字符串,VERBATIM custom_command 会把它当单个参数传给 gcc。
# separate_arguments 转成分号 list,展开时每个 flag 作为独立参数(等价 makefile 的 word splitting)。
separate_arguments(LEOS_C_FLAGS UNIX_COMMAND "${CMAKE_C_FLAGS}")

# 确保 os_symtab_data.c 存在(首次构建时还没有)
# 占位符加 #include os_target.h + #ifdef OS_OPTION_SYMTAB 包裹,与 gen_symtab.py
# 生成的真 data.c 语义一致(OFF 时不产出 g_symtab,避免引入无人引用的死数据)。
if(NOT EXISTS ${LEOS_SYMTAB_SRC})
    file(WRITE ${LEOS_SYMTAB_SRC}
        "/* placeholder — gen_symtab.py will regenerate this */\n"
        "#include \"os_def.h\"\n"
        "#include \"os_target.h\"\n"
        "struct OsSymtabEntry { const void *addr; const char *name; };\n"
        "#define OS_SYMTAB_ENTRY(sym) { (const void *)&(sym), #sym }\n"
        "#ifdef OS_OPTION_SYMTAB\n"
        "OS_SEC_KERNEL_DATA const struct OsSymtabEntry g_symtab[] = { {0, 0} };\n"
        "OS_SEC_KERNEL_DATA const U32 g_symtabCnt = 0;\n"
        "#endif\n"
    )
endif()

# 对象库:编译所有源文件(不含 os_symtab_data.c)
add_library(leos_obj OBJECT ${LEOS_C_SRCS} ${LEOS_ASM_SRCS})

# 两阶段链接:stage1 提取 map → 生成真 data.c → stage2 重编重链
add_custom_command(
    OUTPUT ${LEOS_ELF} ${LEOS_MAP}
    # stage1: 编译占位符 data.o
    COMMAND gcc ${LEOS_C_FLAGS} -c ${LEOS_SYMTAB_SRC} ${LEOS_INCS} -o ${LEOS_SYMTAB_OBJ}
    # stage1: 链接提取 kernel.map(用占位符 data.o)
    COMMAND ld -T ${LEOS_LINK_SCRIPT} -m elf_i386 -Map ${LEOS_MAP}
            $<TARGET_OBJECTS:leos_obj> ${LEOS_SYMTAB_OBJ} -o ${LEOS_ELF_STAGE1}
    # 从 map 生成真 os_symtab_data.c
    COMMAND python3 ${LEOS_SYMTAB_SCRIPT} ${LEOS_MAP} ${LEOS_SYMTAB_SRC}
    # stage2: 重编真 data.o(覆盖占位符版)
    COMMAND gcc ${LEOS_C_FLAGS} -c ${LEOS_SYMTAB_SRC} ${LEOS_INCS} -o ${LEOS_SYMTAB_OBJ}
    # stage2: 最终链接(用真 data.o)
    COMMAND ld -T ${LEOS_LINK_SCRIPT} -m elf_i386 -Map ${LEOS_MAP}
            $<TARGET_OBJECTS:leos_obj> ${LEOS_SYMTAB_OBJ} -o ${LEOS_ELF}
    # 清理 stage1 临时 elf
    COMMAND ${CMAKE_COMMAND} -E rm -f ${LEOS_ELF_STAGE1}
    DEPENDS leos_obj ${LEOS_LINK_SCRIPT} ${LEOS_SYMTAB_SCRIPT}
    COMMENT "Linking os_kernel.elf (two-stage: map → symtab → relink)"
    VERBATIM
    COMMAND_EXPAND_LISTS
)
add_custom_target(os_kernel ALL DEPENDS ${LEOS_ELF})

# dis 目标:生成二进制与反汇编(命令与 makefile 一致)
add_custom_target(dis
    COMMAND objcopy -O binary ${LEOS_ELF} ${LEOS_OUTPUT_DIR}/os_kernel.bin
    COMMAND objdump -d -S ${LEOS_ELF} > ${LEOS_OUTPUT_DIR}/os_kernel.dis
    COMMAND objcopy -O binary -j .MBR ${LEOS_ELF} ${LEOS_OUTPUT_DIR}/os_mbr.bin
    COMMAND objcopy -O binary -j .LOADER ${LEOS_ELF} ${LEOS_OUTPUT_DIR}/os_loader.bin
    COMMAND objcopy -O binary -j .KERNEL_TEXT -j .KERNEL_BSS -j .KERNEL_DATA
            ${LEOS_ELF} ${LEOS_OUTPUT_DIR}/kernel.bin
    DEPENDS os_kernel
    COMMENT "Generating binaries and disassembly"
)
