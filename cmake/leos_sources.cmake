# =============================================================================
# 头文件包含路径与源文件收集
# LEOS_INC_DIRS / LEOS_SRC_DIRS 必须与 makefile 的 INC_DIR / SRC_DIRS
# 逐项、顺序完全一致,否则对象链接顺序不同会导致 .elf 布局不一致。
# 源码已统一迁入 src/,所有路径以 ${CMAKE_SOURCE_DIR}/src 为根。
# =============================================================================

# 头文件包含路径 —— 对齐 makefile INC_DIR(28 条)
set(LEOS_INC_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/kernel/include
    ${CMAKE_SOURCE_DIR}/src/arch/cpu/i386
    ${CMAKE_SOURCE_DIR}/src/arch/hwi/i386
    ${CMAKE_SOURCE_DIR}/src/arch/exc/i386
    ${CMAKE_SOURCE_DIR}/src/arch/idt/i386
    ${CMAKE_SOURCE_DIR}/src/arch/boot
    ${CMAKE_SOURCE_DIR}/src/arch/sys
    ${CMAKE_SOURCE_DIR}/src/arch/timer/i386
    ${CMAKE_SOURCE_DIR}/src/arch/cpu/i386/gdt
    ${CMAKE_SOURCE_DIR}/src/arch/cpu/i386/pgt
    ${CMAKE_SOURCE_DIR}/src/arch/cpu/i386/tss
    ${CMAKE_SOURCE_DIR}/src/arch/cpu/i386/reset
    ${CMAKE_SOURCE_DIR}/src/arch/io/i386
    ${CMAKE_SOURCE_DIR}/src/arch/dev/uart
    ${CMAKE_SOURCE_DIR}/src/lib/include
    ${CMAKE_SOURCE_DIR}/src/kernel/sched
    ${CMAKE_SOURCE_DIR}/src/kernel/task/process
    ${CMAKE_SOURCE_DIR}/src/kernel/tick
    ${CMAKE_SOURCE_DIR}/src/kernel/hwi
    ${CMAKE_SOURCE_DIR}/src/kernel/print
    ${CMAKE_SOURCE_DIR}/src/arch/dev/vga
    ${CMAKE_SOURCE_DIR}/src/arch/dev/kbd
    ${CMAKE_SOURCE_DIR}/src/arch/dev/kbd/i386
    ${CMAKE_SOURCE_DIR}/src/kernel/shell
    ${CMAKE_SOURCE_DIR}/src/kernel/symtab
    ${CMAKE_SOURCE_DIR}/src/debug/include
    ${CMAKE_SOURCE_DIR}/src/kernel/mem
    ${CMAKE_SOURCE_DIR}/src/kernel/mem/fsc
    ${CMAKE_SOURCE_DIR}/src/lib/btmp
    ${CMAKE_SOURCE_DIR}/src/test
)
include_directories(${LEOS_INC_DIRS})

# 源文件搜索目录 —— 对齐 makefile SRC_DIRS(29 条,顺序一致)
set(LEOS_SRC_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/kernel/sched
    ${CMAKE_SOURCE_DIR}/src/kernel/task
    ${CMAKE_SOURCE_DIR}/src/kernel/mem
    ${CMAKE_SOURCE_DIR}/src/kernel/mem/fsc
    ${CMAKE_SOURCE_DIR}/src/kernel/tick
    ${CMAKE_SOURCE_DIR}/src/kernel/hwi
    ${CMAKE_SOURCE_DIR}/src/kernel/ipc/sem
    ${CMAKE_SOURCE_DIR}/src/kernel/ipc/msg
    ${CMAKE_SOURCE_DIR}/src/kernel/task/process
    ${CMAKE_SOURCE_DIR}/src/arch/boot/i386
    ${CMAKE_SOURCE_DIR}/src/arch/boot
    ${CMAKE_SOURCE_DIR}/src/arch/sys
    ${CMAKE_SOURCE_DIR}/src/arch/io/i386
    ${CMAKE_SOURCE_DIR}/src/arch/hwi/i386
    ${CMAKE_SOURCE_DIR}/src/arch/exc/i386
    ${CMAKE_SOURCE_DIR}/src/arch/idt/i386
    ${CMAKE_SOURCE_DIR}/src/arch/timer/i386
    ${CMAKE_SOURCE_DIR}/src/arch/cpu/i386/gdt
    ${CMAKE_SOURCE_DIR}/src/arch/cpu/i386/pgt
    ${CMAKE_SOURCE_DIR}/src/arch/cpu/i386/tss
    ${CMAKE_SOURCE_DIR}/src/lib
    ${CMAKE_SOURCE_DIR}/src/lib/btmp
    ${CMAKE_SOURCE_DIR}/src/arch/cpu/i386
    ${CMAKE_SOURCE_DIR}/src/arch/cpu/i386/reset
    ${CMAKE_SOURCE_DIR}/src/kernel/print
    ${CMAKE_SOURCE_DIR}/src/arch/dev/vga
    ${CMAKE_SOURCE_DIR}/src/arch/dev/uart/i386
    ${CMAKE_SOURCE_DIR}/src/arch/dev/kbd/i386
    ${CMAKE_SOURCE_DIR}/src/kernel/shell
    ${CMAKE_SOURCE_DIR}/src/kernel/symtab
    ${CMAKE_SOURCE_DIR}/src/debug
    ${CMAKE_SOURCE_DIR}/src/test
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

# 排除链接脚本(src/ld_script/os_ld.S 不是源文件)
list(FILTER LEOS_ASM_SRCS EXCLUDE REGEX "ld_script")

# .S 文件用 C 编译器预处理(cmake 默认交给 as,不支持 #include/宏)
set_source_files_properties(${LEOS_ASM_SRCS} PROPERTIES LANGUAGE C)
