# =============================================================================
# 头文件包含路径与源文件收集
# LEOS_INC_DIRS / LEOS_SRC_DIRS 必须与 makefile 的 INC_DIR / SRC_DIRS
# 逐项、顺序完全一致,否则对象链接顺序不同会导致 .elf 布局不一致。
# =============================================================================

# 头文件包含路径 —— 对齐 makefile INC_DIR(28 条)
set(LEOS_INC_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386
    ${CMAKE_SOURCE_DIR}/arch/hwi/i386
    ${CMAKE_SOURCE_DIR}/arch/exc/i386
    ${CMAKE_SOURCE_DIR}/arch/idt/i386
    ${CMAKE_SOURCE_DIR}/arch/boot
    ${CMAKE_SOURCE_DIR}/arch/sys
    ${CMAKE_SOURCE_DIR}/arch/timer/i386
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/gdt
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/pgt
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/tss
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/reset
    ${CMAKE_SOURCE_DIR}/arch/io/i386
    ${CMAKE_SOURCE_DIR}/arch/dev/uart
    ${CMAKE_SOURCE_DIR}/lib/include
    ${CMAKE_SOURCE_DIR}/kernel/sched
    ${CMAKE_SOURCE_DIR}/kernel/task/process
    ${CMAKE_SOURCE_DIR}/kernel/tick
    ${CMAKE_SOURCE_DIR}/kernel/hwi
    ${CMAKE_SOURCE_DIR}/dev/print
    ${CMAKE_SOURCE_DIR}/dev/include
    ${CMAKE_SOURCE_DIR}/arch/dev/vga
    ${CMAKE_SOURCE_DIR}/debug/include
    ${CMAKE_SOURCE_DIR}/kernel/mem
    ${CMAKE_SOURCE_DIR}/kernel/mem/fsc
    ${CMAKE_SOURCE_DIR}/lib/btmp
    ${CMAKE_SOURCE_DIR}/test
)
include_directories(${LEOS_INC_DIRS})

# 源文件搜索目录 —— 对齐 makefile SRC_DIRS(29 条,顺序一致)
set(LEOS_SRC_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel/sched
    ${CMAKE_SOURCE_DIR}/kernel/task
    ${CMAKE_SOURCE_DIR}/kernel/mem
    ${CMAKE_SOURCE_DIR}/kernel/mem/fsc
    ${CMAKE_SOURCE_DIR}/kernel/tick
    ${CMAKE_SOURCE_DIR}/kernel/hwi
    ${CMAKE_SOURCE_DIR}/kernel/ipc/sem
    ${CMAKE_SOURCE_DIR}/kernel/ipc/msg
    ${CMAKE_SOURCE_DIR}/kernel/task/process
    ${CMAKE_SOURCE_DIR}/arch/boot/i386
    ${CMAKE_SOURCE_DIR}/arch/boot
    ${CMAKE_SOURCE_DIR}/arch/sys
    ${CMAKE_SOURCE_DIR}/arch/io/i386
    ${CMAKE_SOURCE_DIR}/arch/hwi/i386
    ${CMAKE_SOURCE_DIR}/arch/exc/i386
    ${CMAKE_SOURCE_DIR}/arch/idt/i386
    ${CMAKE_SOURCE_DIR}/arch/timer/i386
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/gdt
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/pgt
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/tss
    ${CMAKE_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}/lib/btmp
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386
    ${CMAKE_SOURCE_DIR}/arch/cpu/i386/reset
    ${CMAKE_SOURCE_DIR}/dev/print
    ${CMAKE_SOURCE_DIR}/arch/dev/vga
    ${CMAKE_SOURCE_DIR}/arch/dev/uart/i386
    ${CMAKE_SOURCE_DIR}/debug
    ${CMAKE_SOURCE_DIR}/test
)

# 收集所有 .c 文件:按 SRC_DIRS 顺序,每目录 GLOB 后排序,拼接。
# list(SORT) 保证与 make $(wildcard) 的字典序一致。
set(LEOS_C_SRCS "")
foreach(dir ${LEOS_SRC_DIRS})
    file(GLOB dir_c_srcs CONFIGURE_DEPENDS "${dir}/*.c")
    list(SORT dir_c_srcs)
    list(APPEND LEOS_C_SRCS ${dir_c_srcs})
endforeach()

# 收集所有 .S 文件(需 C 预处理的汇编),同样排序
set(LEOS_ASM_SRCS "")
foreach(dir ${LEOS_SRC_DIRS})
    file(GLOB dir_asm_srcs CONFIGURE_DEPENDS "${dir}/*.S")
    list(SORT dir_asm_srcs)
    list(APPEND LEOS_ASM_SRCS ${dir_asm_srcs})
endforeach()

# 排除链接脚本(ld_script/os_ld.S 不是源文件)
list(FILTER LEOS_ASM_SRCS EXCLUDE REGEX "ld_script")

# .S 文件用 C 编译器预处理(cmake 默认交给 as,不支持 #include/宏)
set_source_files_properties(${LEOS_ASM_SRCS} PROPERTIES LANGUAGE C)