#ifndef OS_TARGET_H
#define OS_TARGET_H

/*
 * 目标架构 facade
 *
 * 本文件不硬编码架构，改为引入具体架构的特性配置头 os_feature.h。
 * 该头由各架构自己提供（放 config/<arch>/ 下），定义 ARCH_<arch>
 * 及该架构支持的 OS_OPTION_* 功能特性宏。makefile/cmake 的 INC_DIR 里
 * 放入哪个 config/<arch>/ 目录，就选中哪个架构，源码零改动切换。
 */

#include "os_feature.h"

#ifndef ARCH_i386
#error "Unsupported architecture. Provide os_feature.h (in config/<arch>/) with ARCH_<arch> defined."
#endif

#endif
