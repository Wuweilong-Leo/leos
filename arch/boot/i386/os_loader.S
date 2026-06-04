#include "os_cpu_i386.h"

/* === 实模式代码（必须在 Loader 开头）=== */
    .section .os.loader.text.real, "ax"
    .global OsLoaderStart
    .code16
OsLoaderStart:
    /* 不获取物理内存大小了，直接赋值32M */
    
    /* === 在实模式用 BIOS 加载内核到 0xD000 === */
    /* 内核约 54KB (107 扇区)，一次读取 128 扇区足够 */
    
    movw $0x0000, %ax
    movw %ax, %es
    movw %ax, %ds
    
    movw $128, pkt_sectors  /* 读取 128 扇区 */
    movl $0x0000D000, pkt_buffer /* 线性地址 0xD000 */
    movl $OS_KERNEL_SEC_ID, pkt_lba
    movl $0, pkt_lba+4
    
    movb $0x42, %ah
    movw $disk_addr_packet, %si
    int $0x13
    jc disk_read_error
    
    /* 内核已加载，继续启动 */
    jmp OsEnableProtectMode
    
disk_read_error:
    /* 简单错误处理：死循环 */
    cli
    hlt
    jmp disk_read_error

OsEnableProtectMode:
    /* 打开a20 gate */
    inb $0x92, %al
    or $0x2, %al
    outb %al, $0x92

    /* 加载gdt */
    lgdt g_gdtInfo

    /* 打开cr0的pe位 */
    movl %cr0, %eax
    or $0x1, %eax
    movl %eax, %cr0

    nop
    nop
    nop

    /* 实模式的指令已经进流水线了，要刷掉 */
    ljmp $OS_SELECTOR_K_CODE, $OsPmodeStart

/* === 保护模式代码 === */
    .section .os.loader.text, "ax"
    .code32
OsPmodeStart:
    movw $OS_SELECTOR_K_DATA, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl $_os_boot_sp_end, %esp
    movw $OS_SELECTOR_K_VEDIO, %ax
    movw %ax, %gs

    call OsSetupPgt

    /* 内核已在实模式加载到 0xD000，无需再读磁盘 */

    sgdt g_gdtInfo

    call OsModGdt

    /* 开启页映射 */
    movl $g_pgd, %eax
    movl %eax, %cr3
    movl %cr0, %eax
    orl  $0x80000000, %eax
    movl %eax, %cr0

    lgdt g_gdtInfo

    ljmp $OS_SELECTOR_K_CODE, $OsEnterKernel
OsEnterKernel:
    movl $_os_kernel_sp_end, %esp
    call main 

/* 磁盘地址包 (用于 LBA 扩展读取) */
    .section .os.loader.data, "a"
    .align 4
disk_addr_packet:
    .byte 0x10               /* 包大小 = 16 字节 */
    .byte 0                  /* 保留 */
pkt_sectors:
    .word 200                /* 要读取的扇区数 */
pkt_buffer:
    .long 0x0000D000         /* 目标缓冲区地址 (段:偏移格式) */
pkt_lba:
    .quad OS_KERNEL_SEC_ID   /* 起始 LBA 扇区号 */

    