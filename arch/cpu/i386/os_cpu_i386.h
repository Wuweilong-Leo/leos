#ifndef OS_CPU_I386_H
#define OS_CPU_I386_H
#define OS_PG_SIZE 4096
/* pc一开始在此处，bios把mbr程序搬到此地址，开始执行mbr */
#define OS_MBR_BASE 0x7C00
/* 显存的起始地址 */
#define OS_VIDEO_MEM_BASE 0x000B8000
#define OS_VIDEO_MEM_GS (OS_VIDEO_MEM_BASE >> 4)
#define OS_VIDEO_MEM_END 0x000BFFFF
#define OS_VIDEO_MEM_SIZE (OS_VIDEO_MEM_END - OS_VIDEO_MEM_BASE)
#define OS_VIDEO_MEM_PGS_NUM (OS_VIDEO_MEM_SIZE / OS_PG_SIZE)
/* loader的起始扇区号 */
#define OS_LOADER_START_SEC_ID 0x2
/* loader的sec数 */
#define OS_LOADER_SEC_NUM 2
/* 端口号 */
#define OS_DISK_SEC_CNT_PORT 0x1F2
#define OS_DISK_LBA_LOW_PORT 0x1F3
#define OS_DISK_LBA_MID_PORT 0x1F4
#define OS_DISK_LBA_HIGH_PORT 0x1F5
#define OS_DISK_DEV_PORT 0x1F6
#define OS_DISK_CMD_STA_PORT 0x1F7
#define OS_DISK_DEV_LBA_MASK 0xE0
#define OS_DISK_CMD_RD 0x20
#define OS_DISK_STA_BUSY_MASK 0x80
#define OS_DISK_STA_RDY_MASK 0x08
#define OS_DISK_RD_PORT 0x1F0
#define OS_KERNEL_SEC_ID 0x9

#define OS_RPL0 0
#define OS_RPL1 1
#define OS_RPL2 2
#define OS_RPL3 3
#define OS_TI_GDT 0
#define OS_TI_LDT 1
#define OS_SELECTOR_K_CODE ((1 << 3) + (OS_TI_GDT << 2) + OS_RPL0)
#define OS_SELECTOR_K_DATA ((2 << 3) + (OS_TI_GDT << 2) + OS_RPL0)
#define OS_SELECTOR_K_VEDIO ((3 << 3) + (OS_TI_GDT << 2) + OS_RPL0)
#define OS_SELECTOR_U_CODE ((5 << 3) + (OS_TI_GDT << 2) + OS_RPL3)
#define OS_SELECTOR_U_DATA ((6 << 3) + (OS_TI_GDT << 2) + OS_RPL3)

#define OS_FAST_SAVE_FLAG 0x0U
#define OS_ALL_SAVE_FLAG 0x1U

#define OS_EFLAGS_MBS (1 << 1)
#define OS_EFLAGS_IF_1 (1 << 9)
#define OS_EFLAGS_IF_0 (0 << 9)
#define OS_EFLAGS_IOPL_0 (0 << 12)
#define OS_PROCESS_EFLAGS (OS_EFLAGS_IOPL_0 | OS_EFLAGS_IF_0 | OS_EFLAGS_MBS)
#endif