#include "os_cpu_i386.h"

/* === 实模式代码（必须在 Loader 开头）=== */
    .section .os.loader.text.real, "ax"
    .global OsLoaderStart
    .code16
OsLoaderStart:
    /*
     * 实模式用 BIOS int 0x13 AH=42h 循环读取内核
     *
     * kernel.bin 最大 0x60000 字节 (768 扇区)
     * 每批读 64 扇区，循环直到读完
     *
     * DAP 放在 0x500 (BIOS 数据区之后的安全区域)
     * buffer 用段:偏移格式: offset 固定为 0xD000，segment 递增
     *
     * 每批 64 扇区 = 32768 字节 = 0x8000
     *   段递增 0x0800 (32768/16)
     *   offset 始终 0xD000，不跨越 64KB 边界
     *   0xD000 + 0x8000 = 0x15000 → 下一批 segment=0x0800 offset=0xD000 → 0x15000 ✓
     *   0xD000 + 0x8000*2 = 0x1D000 → segment=0x1000 offset=0xD000 → 0x1D000 ✓
     *   768扇区 / 64 = 12 批
     */
    
    movw $0x0000, %ax
    movw %ax, %es
    movw %ax, %ds
    
    /* 初始化 DAP 模板 (0x500) */
    movw $0x0500, %si
    movb $0x10, (%si)           /* 包大小 = 16 */
    movb $0x00, 1(%si)          /* 保留 = 0 */
    
    /* BX = 当前 segment (从 0 开始) */
    xorw %bx, %bx
    /* CX = 剩余扇区数 */
    movw $768, %cx               /* 最大 768 扇区 */
    /* DX 低位 = 当前 LBA (从 OS_KERNEL_SEC_ID 开始) */
    /* 用内存存 LBA, 因为 DX 要给 DL=0x80 */
    
    /* 把起始 LBA 存到 0x508 (DAP 的 LBA 字段) */
    movl $OS_KERNEL_SEC_ID, 0x508
    movl $0, 0x50C
    
read_loop:
    /* 计算本批扇区数: min(CX, 64) */
    movw %cx, %ax
    cmpw $64, %ax
    jbe 1f
    movw $64, %ax
1:
    /* AX = 本批扇区数, 保存到栈 */
    pushw %ax
    movw %ax, 0x502             /* DAP sectors 字段 */
    
    /* 设置 buffer: offset=0xD000, segment=BX */
    movw $0xD000, 0x504         /* offset */
    movw %bx, 0x506             /* segment */
    
    /* 调用 int 0x13 */
    movw $0x0500, %si
    movb $0x42, %ah
    movb $0x80, %dl
    int $0x13
    popw %ax                    /* 恢复本批扇区数 */
    jc disk_read_error
    
    /* 更新剩余扇区 */
    subw %ax, %cx
    cmpw $0, %cx
    jbe read_done
    
    /* 更新 LBA: 0x508 += AX */
    pushw %ax
    movzwl %ax, %eax
    addl %eax, 0x508
    popw %ax
    
    /* 更新 segment: BX += 64*512/16 = 0x800 */
    addw $0x0800, %bx
    
    jmp read_loop

read_done:
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

disk_read_error:
    cli
    hlt
    jmp disk_read_error

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
