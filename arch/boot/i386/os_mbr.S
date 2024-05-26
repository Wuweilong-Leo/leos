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
    movw $_os_boot_sp_end, %sp
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

os_mbr_end:
    jmp os_mbr_end

    pushl $OS_LOADER_SECS_NUM
    pushl $OS_LOADER_START_SEC_ID
    pushl $OS_LOADER_BASE
// call os_read_disk_m16
    
    jmp os_loader_start