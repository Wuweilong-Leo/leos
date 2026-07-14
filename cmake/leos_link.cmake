# =============================================================================
# 对象库 + 手工 ld 链接 + 符号表生成 + dis 目标
# 链接命令与 makefile 对齐;dis 命令与 makefile 一致。
#
# 符号表生成流程:
#   首次 make → 用占位 os_symtab_data.c 编译+链接 → 生成 kernel.map
#             → 链接后 gen_symtab 从 map 生成真正的 os_symtab_data.c
#   再次 make → os_symtab_data.c 变化 → 重新编译+链接 → 符号表生效
# =============================================================================

set(LEOS_OUTPUT_DIR  ${CMAKE_BINARY_DIR})
set(LEOS_ELF         ${LEOS_OUTPUT_DIR}/os_kernel.elf)
set(LEOS_MAP         ${LEOS_OUTPUT_DIR}/kernel.map)
set(LEOS_LINK_SCRIPT ${CMAKE_SOURCE_DIR}/src/ld_script/os_ld.S)
set(LEOS_SYMTAB_SRC  ${CMAKE_SOURCE_DIR}/src/kernel/symtab/os_symtab_data.c)
set(LEOS_SYMTAB_SCRIPT ${CMAKE_SOURCE_DIR}/tools/gen_symtab.py)

# 确保 os_symtab_data.c 存在(首次构建时可能还没有 kernel.map)
if(NOT EXISTS ${LEOS_SYMTAB_SRC})
    file(WRITE ${LEOS_SYMTAB_SRC}
        "/* placeholder — gen_symtab.py will regenerate this */\n"
        "#include \"os_symtab_external.h\"\n"
        "#include \"os_def.h\"\n"
        "\n"
        "OS_SEC_KERNEL_DATA const struct OsSymtabEntry g_symtab[] = { {0, 0} };\n"
        "OS_SEC_KERNEL_DATA const U32 g_symtabCnt = 0;\n"
    )
endif()

# 对象库:编译所有源文件(含 os_symtab_data.c)
add_library(leos_obj OBJECT ${LEOS_C_SRCS} ${LEOS_ASM_SRCS})

# 自定义链接:用 ld 而非 gcc 链接
add_custom_command(
    OUTPUT ${LEOS_ELF} ${LEOS_MAP}
    COMMAND ld -T ${LEOS_LINK_SCRIPT} -m elf_i386 -Map ${LEOS_MAP}
            $<TARGET_OBJECTS:leos_obj> -o ${LEOS_ELF}
    DEPENDS leos_obj ${LEOS_LINK_SCRIPT}
    COMMENT "Linking os_kernel.elf"
    VERBATIM
    COMMAND_EXPAND_LISTS
)
add_custom_target(os_kernel ALL DEPENDS ${LEOS_ELF})

# 链接后:从生成的 map 文件重新生成符号表源文件
# 下次 make 时如果 symtab_data.c 有变化,会触发重新编译+链接
add_custom_command(
    OUTPUT ${LEOS_SYMTAB_SRC}.stamp
    COMMAND python3 ${LEOS_SYMTAB_SCRIPT} ${LEOS_MAP} ${LEOS_SYMTAB_SRC}
    COMMAND ${CMAKE_COMMAND} -E touch ${LEOS_SYMTAB_SRC}.stamp
    DEPENDS ${LEOS_SYMTAB_SCRIPT}
    COMMENT "Generating symbol table from kernel.map"
)
add_custom_target(gen_symtab ALL DEPENDS ${LEOS_SYMTAB_SRC}.stamp)
add_dependencies(gen_symtab os_kernel)

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
