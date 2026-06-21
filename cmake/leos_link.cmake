# =============================================================================
# 对象库 + 手工 ld 链接 + dis 目标
# 链接命令与 makefile 对齐;dis 命令与 makefile 对齐。
# =============================================================================

set(LEOS_OUTPUT_DIR  ${CMAKE_BINARY_DIR})
set(LEOS_ELF         ${LEOS_OUTPUT_DIR}/os_kernel.elf)
set(LEOS_MAP         ${LEOS_OUTPUT_DIR}/kernel.map)
set(LEOS_LINK_SCRIPT ${CMAKE_SOURCE_DIR}/ld_script/os_ld.S)

# 不直接生成可执行文件:用 ld 手工链接(不走 gcc 默认 crt0/libc)
add_library(leos_obj OBJECT ${LEOS_C_SRCS} ${LEOS_ASM_SRCS})

# 自定义链接:用 ld 而非 gcc 链接
add_custom_command(
    OUTPUT ${LEOS_ELF}
    COMMAND ld -T ${LEOS_LINK_SCRIPT} -m elf_i386 -Map ${LEOS_MAP}
            $<TARGET_OBJECTS:leos_obj> -o ${LEOS_ELF}
    DEPENDS leos_obj ${LEOS_LINK_SCRIPT}
    COMMENT "Linking os_kernel.elf"
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