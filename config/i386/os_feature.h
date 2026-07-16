#ifndef OS_FEATURE_H
#define OS_FEATURE_H

/*
 * i386 架构特性配置（每架构一份，放 config/<arch>/ 下，由
 * kernel/include/os_target.h 经 include 路径选中。makefile/cmake 的
 * INC_DIR 里放哪个 config/<arch>/ 就用哪个架构的配置，源码零改动切换）
 *
 * - ARCH_i386：架构标识，供 os_cpu.h / os_hwi.h / os_exc.h 等 facade
 *   用 #if defined(ARCH_i386) 选对应的 os_*_i386.h
 * - OS_OPTION_*：本架构支持的功能特性宏，源码用 #ifdef 据此裁剪
 */

#define ARCH_i386

/* 优先级继承：BINARY_MUTEX 信号量支持优先级继承，防止优先级反转 */
#define OS_OPTION_PRIO_INHERIT

/*
 * 符号表（symtab）：内核符号表查表，供 shell 的 syms/addr2name 命令使用。
 * 关闭后 os_symtab.c 函数体与 os_shell.c 调用点被裁掉，
 * os_symtab_data.c 的数据表成为无人引用符号（不报错，仅占镜像体积）。
 */
#define OS_OPTION_SYMTAB

/*
 * 递归互斥锁：BINARY_MUTEX 支持同任务多次 Pend（nestCnt 嵌套计数）。
 * 注：本宏已收编为 OS_OPTION_ 命名（原游离宏 OS_SEM_BIN_SUPPORT_RECUR），
 *     但此处刻意【不定义】，功能保持 OFF——当前无递归锁测试用例，
 *     贸然开启只能靠"不回归"验证不充分。开启 + 补测试作为后续独立项。
 */

#endif /* OS_FEATURE_H */
