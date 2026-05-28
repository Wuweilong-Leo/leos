cmake_minimum_required(VERSION 3.16)
project(leos LANGUAGES C)

# =============================================================================
# 编译器设置
# =============================================================================
set(CMAKE_C_COMPILER gcc)

# 强制 32 位编译，freestanding 环境（不依赖标准库），关闭栈保护器
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32 -std=c11 -ffreestanding -fno-stack-protector")

# =============================================================================
# 头文件包含路径
# =============================================================================
set(LEOS_INC_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386
    ${CMAKE_SOURCE_DIR}/arch/hwi/i386
    ${CMAKE_SOURCE_DIR}/arch/boot
    ${CMAKE_SOURCE_DIR}/arch/sys
    ${CMAKE_SOURCE_DIR}/arch/timer/i386
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/gdt
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/pgt
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/tss
    ${CMAKE_SOURCE_DIR}/arch/io/i386
    ${CMAKE_SOURCE_DIR}/lib/include
    ${CMAKE_SOURCE_DIR}/kernel/sched
    ${CMAKE_SOURCE_DIR}/kernel/task/process
    ${CMAKE_SOURCE_DIR}/kernel/tick
    ${CMAKE_SOURCE_DIR}/dev/print
    ${CMAKE_SOURCE_DIR}/dev/include
    ${CMAKE_SOURCE_DIR}/debug/include
    ${CMAKE_SOURCE_DIR}/kernel/mem
    ${CMAKE_SOURCE_DIR}/kernel/mem/fsc
    ${CMAKE_SOURCE_DIR}/lib/btmp
)

include_directories(${LEOS_INC_DIRS})

# =============================================================================
# 源文件收集
# =============================================================================
set(LEOS_SRC_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel/sched
    ${CMAKE_SOURCE_DIR}/kernel/task
    ${CMAKE_SOURCE_DIR}/kernel/mem
    ${CMAKE_SOURCE_DIR}/kernel/mem/fsc
    ${CMAKE_SOURCE_DIR}/kernel/tick
    ${CMAKE_SOURCE_DIR}/kernel/ipc/sem
    ${CMAKE_SOURCE_DIR}/kernel/task/process
    ${CMAKE_SOURCE_DIR}/arch/boot/i386
    ${CMAKE_SOURCE_DIR}/arch/boot
    ${CMAKE_SOURCE_DIR}/arch/sys
    ${CMAKE_SOURCE_DIR}/arch/io/i386
    ${CMAKE_SOURCE_DIR}/arch/hwi/i386
    ${CMAKE_SOURCE_DIR}/arch/timer/i386
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/gdt
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/pgt
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/tss
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/lib/btmp
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386
    ${CMAKE_SOURCE_DIR}/dev/print
    ${CMAKE_SOURCE_DIR}/debug
)

# 收集所有 .c 文件
set(LEOS_C_SRCS "")
foreach(dir ${LEOS_SRC_DIRS})
    file(GLOB dir_c_srcs "${dir}/*.c")
    list(APPEND LEOS_C_SRCS ${dir_c_srcs})
endforeach()

# 收集所有 .S 文件（需要 C 预处理的汇编）
set(LEOS_ASM_SRCS "")
foreach(dir ${LEOS_SRC_DIRS})
    file(GLOB dir_asm_srcs "${dir}/*.S")
    list(APPEND LEOS_ASM_SRCS ${dir_asm_srcs})
endforeach()

# 排除链接脚本（ld_script/os_ld.S 不是源文件）
list(FILTER LEOS_ASM_SRCS EXCLUDE REGEX "ld_script")

# =============================================================================
# 关键点：让 .S 文件使用 C 编译器预处理
# =============================================================================
# CMake 默认把 .S 交给 as 直接汇编，不支持 #include。
# 这里强制用 gcc -c 来编译 .S，这样 #include 和宏都能正常展开。
set_source_files_properties(${LEOS_ASM_SRCS} PROPERTIES LANGUAGE C)

# =============================================================================
# 构建对象库
# =============================================================================
# 不直接生成可执行文件，因为我们要用 ld 手动链接（不用 gcc 默认的 crt0/libc）
add_library(leos_obj OBJECT ${LEOS_C_SRCS} ${LEOS_ASM_SRCS})

# =============================================================================
# 自定义链接：用 ld 而非 gcc 链接
# =============================================================================
set(LEOS_OUTPUT_DIR ${CMAKE_BINARY_DIR})
set(LEOS_ELF        ${LEOS_OUTPUT_DIR}/os_kernel.elf)
set(LEOS_MAP        ${LEOS_OUTPUT_DIR}/kernel.map)
set(LEOS_LINK_SCRIPT ${CMAKE_SOURCE_DIR}/ld_script/os_ld.S)

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

# =============================================================================
# dis 目标：生成二进制和反汇编
# =============================================================================
add_custom_target(dis
    COMMAND objcopy -O binary ${LEOS_ELF} ${LEOS_OUTPUT_DIR}/os_kernel.bin
    COMMAND objdump -d -S ${LEOS_ELF} > ${LEOS_OUTPUT_DIR}/os_kernel.dis
    COMMAND objcopy -O binary -j .MBR ${LEOS_ELF} ${LEOS_OUTPUT_DIR}/os_mbr.bin
    COMMAND objcopy -O binary -j .LOADER ${LEOS_ELF} ${LEOS_OUTPUT_DIR}/os_loader.bin
    COMMAND objcopy -O binary -j .KERNEL_TEXT -j .KERNEL_BSS -j .KERNEL_DATA ${LEOS_ELF} ${LEOS_OUTPUT_DIR}/kernel.bin
    DEPENDS os_kernel
    COMMENT "Generating binaries and disassembly"
)
