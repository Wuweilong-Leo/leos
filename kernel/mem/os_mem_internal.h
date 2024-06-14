#ifndef OS_MEM_INTERNAL_H
#define OS_MEM_INTERNAL_H
#include "os_mem_external.h"

#define OS_TOTAL_PHY_MEM_SIZE (32 * 1024 * 1024)
/*
 * 0x100000 内核镜像
 * 0x1000 页目录
 * 256 * 0x1000 页表
 * 3 * 0x1000 bitmap
 */
#define OS_USED_PHY_MEM_SIZE (0x100000 + 0x1000 + 256 * 0x1000)
#define OS_GET_FREE_PHY_MEM_SIZE(usedMem) (OS_TOTAL_PHY_MEM_SIZE - (usedMem))
#define OS_GET_FREE_KERNEL_PHY_MEM_SIZE(totalFreeMem) ((totalFreeMem) >> 1)
#define OS_GET_FREE_USR_PHY_MEM_SIZE(totalFreeMem) ((totalFreeMem) >> 1)
#define OS_GET_PG_NUM_BY_MEM_SIZE(memSize) (OS_ROUND_UP(memSize, OS_PG_SIZE)/ OS_PG_SIZE)

#define OS_MEM_BTMP_BASE 0xFD000
#define OS_MEM_BTMP0_BASE OS_MEM_BTMP_BASE
#define OS_MEM_BTMP1_BASE (OS_MEM_BTMP0_BASE + OS_PG_SIZE)
#define OS_MEM_BTMP2_BASE (OS_MEM_BTMP1_BASE + OS_PG_SIZE)

#define OS_KERNEL_VIR_MEM_BASE 0xC0100000
#endif