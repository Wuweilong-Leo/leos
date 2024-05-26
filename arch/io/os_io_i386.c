#include "os_def.h"
#include "os_cpu_i386.h"
#include "os_io_i386.h"

OS_SEC_MBR_TEXT void os_read_disk_m16(uintptr_t dst, uint32_t sec_id, uint32_t sec_cnt)
{
    volatile uint16_t disk_status;
    volatile uint16_t data;
    uintptr_t addr;
    uint32_t read_times;
    uint32_t i;

    os_outb(OS_DISK_SEC_CNT_PORT, (uint8_t)sec_cnt);
    os_outb(OS_DISK_LBA_LOW_PORT, OS_GET_BYTE_BY_IDX(sec_id, 0));
    os_outb(OS_DISK_LBA_MID_PORT, OS_GET_BYTE_BY_IDX(sec_id, 8));
    os_outb(OS_DISK_LBA_HIGH_PORT, OS_GET_BYTE_BY_IDX(sec_id, 16));
    os_outb(OS_DISK_DEV_PORT, OS_DISK_DEV_LBA_MASK);

    /* 开始读取 */
    os_outb(OS_DISK_CMD_STA_PORT, OS_DISK_CMD_RD);

    /* 等到disk准备好数据 */
    while ((os_inb(OS_DISK_CMD_STA_PORT) & (OS_DISK_STA_BUSY_MASK | OS_DISK_STA_RDY_MASK)) != OS_DISK_STA_RDY_MASK) {}

    /* 一共读取secsCnt个扇区，一扇区512B，一次读2B */
    read_times = sec_cnt * (OS_DISK_SEC_SIZE / 2);

    for (i = 0; i < read_times; i++) {
        addr = (uintptr_t)((uint32_t)dst + 2 * i);
        /* 读取2字节 */
        data = os_inw(OS_DISK_RD_PORT);
        *(uint16_t *)addr = data;
    }
}