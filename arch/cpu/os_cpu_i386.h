#ifndef OS_CPU_I386_H
#define OS_CPU_I386_H
/* pc一开始在此处，bios把mbr程序搬到此地址，开始执行mbr */
#define OS_MBR_BASE 0x7C00
/* 显存的起始地址 */
#define OS_VIDEO_MEM_BASE 0xB800
/* loader的起始扇区号 */
#define OS_LOADER_START_SEC_ID 0x2
/* loader被加载的地址 */
#define OS_LOADER_BASE 0x900
/* loader的sec数 */
#define OS_LOADER_SECS_NUM 0x4
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
#define OS_DISK_SEC_SIZE 512
#define OS_DISK_RD_PORT 0x1F0

#endif