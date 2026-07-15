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

#endif /* OS_FEATURE_H */
