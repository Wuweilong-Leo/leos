#include "os_cpu_i386.h"

.code16
.section .os.mbr.text, "ax"
.global os_mbr

/* 实模式 */
os_mbr:
    movw %cs, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw $_os_kernel_sp_end, %sp
    movw $OS_VIDEO_MEM_BASE, %ax
    movw %ax, %gs
/* 
 * 一个中断可以有多个功能号，完成不同功能。
 *
 * ah 功能号=0x06 表示清屏功能
 * al 上卷的行数, 0 表示全部
 * bh 表示上卷行属性，可忽略
 * (cl,ch),(dl,dh)表示窗口的左上角坐标和窗口右上角坐标
 */
    movw $0x600, %ax
    movw $0x700, %bx
    movw $0, %cx
    movw $0x184F, %dx
    int $0x10

    movb $'1', %gs:0x00
    movb $0xA4, %gs:0x01
    movb $' ', %gs:0x02
    movb $0XA4, %gs:0x03
    movb $'M', %gs:0x04
    movb $0XA4, %gs:0x05
    movb $'B', %gs:0x06
    movb $0XA4, %gs:0x07
    movb $'R', %gs:0x08
    movb $0XA4, %gs:0x09

    movl $OS_LOADER_START_SEC_ID, %eax /* 扇区ID */
    movw $_os_loader_start, %bx /* loader基地址 */
    movw $OS_LOADER_SEC_NUM, %cx /* 扇区数 */

    call os_read_disk_m16
    
    jmp os_loader_start

.code16
.section .os.mbr.text, "ax"
.global os_read_disk_m16

os_read_disk_m16:
    /* 之后的操作要更改eax, cx, 先保存起来后面恢复 */
    movl %eax, %esi
    movw %cx, %di

    /* 写入要读取的扇区数目 */
    movw $OS_DISK_SEC_CNT_PORT, %dx
    movb %cl, %al
    outb %al, %dx
    movl %esi, %eax

    movw $OS_DISK_LBA_LOW_PORT, %dx
    outb %al, %dx

    movb $0x8, %cl 
    shr %cl, %eax
    movw $OS_DISK_LBA_MID_PORT, %dx
    outb %al, %dx

    shr %cl, %eax
    movw $OS_DISK_LBA_HIGH_PORT, %dx
    outb %al, %dx

    shr %cl, %eax
    and $0x0F, %al
    or $OS_DISK_DEV_LBA_MASK, %al
    movw $OS_DISK_DEV_PORT, %dx
    outb %al, %dx

    movw $OS_DISK_CMD_STA_PORT, %dx
    movb $OS_DISK_CMD_RD, %al
    outb %al, %dx

os_disk_not_ready:
    nop
    inb %dx, %al
    and $0x88, %al
    cmp $0x08, %al
    jne os_disk_not_ready

    movw %di, %ax
    movw $256, %dx
    mul %dx
    movw %ax, %cx
    movw $OS_DISK_RD_PORT, %dx

os_disk_read_next:
    inw %dx, %ax
    movw %ax, 0(%bx)
    add $2, %bx
    loop os_disk_read_next
    ret 
