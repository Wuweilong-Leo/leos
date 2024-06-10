#include "os_cpu_i386.h"
    .section .os.loader.text, "ax"
    .global OsLoaderStart
    .code16
OsLoaderStart:
    /* 不获取物理内存大小了，直接赋值32M */
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
    
    .code32
OsPmodeStart:
    movw $OS_SELECTOR_K_DATA, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl $_os_kernel_sp_end, %esp
    movw $OS_SELECTOR_K_VEDIO, %ax
    movw %ax, %gs

    call OsSetupPgt

    pushl $0xD000
    pushl $200
    pushl $OS_KERNEL_SEC_ID
    call OsReadDiskM32
    addl $12, %esp

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
    addl $0xc0000000, %esp
    call main 





    

    