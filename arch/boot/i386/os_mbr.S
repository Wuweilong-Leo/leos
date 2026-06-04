#include "os_cpu_i386.h"

/*
 * MBR - 支持多种磁盘读取模式
 *
 * 读取策略:
 *   1. BIOS LBA 扩展读 (int 0x13 AH=42h) — 硬盘/现代 BIOS
 *   2. BIOS CHS 读 (int 0x13 AH=02h) — 软盘/老硬盘 fallback
 *   3. 端口 I/O LBA28 — 保护模式用 (OsReadDiskM16 保留)
 */

.code16
.section .os.mbr.text, "ax"
.global OsMbr

OsMbr:
    movw %cs, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw $_os_boot_sp_end, %sp
    movw $OS_VIDEO_MEM_GS, %ax
    movw %ax, %gs

    /* 清屏 */
    movw $0x600, %ax
    movw $0x700, %bx
    movw $0, %cx
    movw $0x184F, %dx
    int $0x10

    /* 打印 "1 MBR" */
    movb $'1', %gs:0x00
    movb $0xA4, %gs:0x01
    movb $' ', %gs:0x02
    movb $0xA4, %gs:0x03
    movb $'M', %gs:0x04
    movb $0xA4, %gs:0x05
    movb $'B', %gs:0x06
    movb $0xA4, %gs:0x07
    movb $'R', %gs:0x08
    movb $0xA4, %gs:0x09

    /* ================================================
     * 读取 Loader
     * DAP 放在 0x500 (BIOS 数据区之后的安全区域)
     * ================================================ */
    movw $0x0500, %si
    movb $0x10, (%si)           /* dapSize = 16 */
    movb $0x00, 1(%si)          /* reserved */
    movw $OS_LOADER_SEC_NUM, 2(%si)  /* sectors */
    movw $_os_loader_start, 4(%si)  /* offset */
    movw $0x0000, 6(%si)       /* segment = 0 */
    movl $OS_LOADER_START_SEC_ID, 8(%si)  /* LBA low 32-bit */
    movl $0x00000000, 12(%si)  /* LBA high 32-bit */

    /* 1) 试硬盘 LBA 扩展读 (DL=0x80) */
    movb $0x42, %ah
    movb $0x80, %dl
    int $0x13
    jnc OsMbrLoaderOk

    /* 2) 试软盘 LBA 扩展读 (DL=0x00) */
    movw $0x0500, %si
    movb $0x42, %ah
    movb $0x00, %dl
    int $0x13
    jnc OsMbrLoaderOk

    /* 3) CHS fallback: 用 BIOS 获取驱动器几何参数后读 */
    /* 先试硬盘 */
    movb $0x80, %dl
    call OsReadChsFallback
    test %al, %al
    jz OsMbrLoaderOk

    /* 试软盘 */
    movb $0x00, %dl
    call OsReadChsFallback

OsMbrLoaderOk:
    jmp OsLoaderStart


/* -------------------------------------------------------
 * OsReadChsFallback: CHS fallback 读取 Loader
 *   用 BIOS int 0x13 AH=08h 获取驱动器几何参数
 *   然后用 AH=02h CHS 模式逐扇区读取
 *   DL = 驱动器号
 * ------------------------------------------------------- */
.code16
.section .os.mbr.text, "ax"
OsReadChsFallback:
    /* 保存 DL */
    pushw %dx

    /* 获取驱动器参数: int 0x13 AH=08h */
    movb $0x08, %ah
    int $0x13
    jc OsChsFallbackFail

    /* 返回: CH = max cylinder low 8, CL[5:0] = spt, CL[7:6] = cyl high 2 bits */
    /*        DH = max head number (0-based), DL = drive count */
    /* spt = CL & 0x3F, heads = DH + 1 */
    andw $0x3F, %cx            /* CX = spt */
    movw %cx, %di               /* DI = spt */
    incb %dh                    /* DH = heads */
    movb %dh, %bh               /* BH = heads */

    /* 恢复 DL */
    popw %dx                    /* DX = drive */

    /* 计算 LBA → CHS */
    /* AX = LBA (只取低16位), BH = heads, DI = spt */
    movw $OS_LOADER_START_SEC_ID, %ax  /* LBA */

    /* heads_per_cyl = heads * spt */
    movb %bh, %al
    cbw                          /* AX = heads */
    mulw %di                     /* AX = heads * spt = heads_per_cyl */
    movw %ax, %bp                /* BP = heads_per_cyl */

    /* cylinder = LBA / heads_per_cyl */
    movw $OS_LOADER_START_SEC_ID, %ax
    xorw %dx, %dx
    divw %bp                     /* AX = cylinder, DX = temp */

    /* temp / spt = head */
    movw %dx, %ax
    xorw %dx, %dx
    divw %di                     /* AX = head, DX = sector-1 */

    /* 构建 CHS 参数 */
    /* CH = cylinder & 0xFF */
    /* CL = (sector) | ((cylinder >> 8) << 6) */
    /* DH = head */
    pushw %ax                   /* 保存 head */
    pushw %dx                   /* 保存 sector-1 */

    /* 先算 cylinder (之前 div 结果在 AX, 但被 head 的 div 覆盖了) */
    /* 重新算 cylinder */
    movw $OS_LOADER_START_SEC_ID, %ax
    xorw %dx, %dx
    divw %bp                     /* AX = cylinder, DX 不用 */
    movb %al, %ch               /* CH = cylinder low 8 */
    movb %ah, %bl               /* BL = cylinder high byte */

    popw %dx                    /* DX = sector-1 */
    popw %ax                    /* AX = head */

    /* DH = head */
    movb %al, %dh

    /* CL = sector | (cyl_high << 6) */
    movb %dl, %cl               /* sector-1 */
    incb %cl                    /* sector (1-based) */
    /* cylinder high 2 bits → CL[7:6] */
    movb %bl, %al
    shlb $6, %al
    orb %al, %cl

    /* 读取: AL = 扇区数, ES:BX = buffer */
    mov $2, %al
    mov $_os_loader_start, %bx
    xor %ax, %ax
    mov %ax, %es

    movb $0x02, %ah
    int $0x13
    jc OsChsFallbackFail2

    movb $0, %al
    ret

OsChsFallbackFail:
    popw %dx
OsChsFallbackFail2:
    movb $1, %al
    ret


/* -------------------------------------------------------
 * OsReadDiskM16: 端口 I/O LBA28 读取 (保留，供保护模式 M32 调用参考)
 * ------------------------------------------------------- */
.code16
.section .os.mbr.text, "ax"
.global OsReadDiskM16

OsReadDiskM16:
    movl %eax, %esi
    movw %cx, %di

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

OsDiskNotRdy:
    nop
    inb %dx, %al
    and $0x88, %al
    cmp $0x08, %al
    jne OsDiskNotRdy

    movw %di, %ax
    movw $256, %dx
    mul %dx
    movw %ax, %cx
    movw $OS_DISK_RD_PORT, %dx

OsDiskReadNext:
    inw %dx, %ax
    movw %ax, 0(%bx)
    add $2, %bx
    loop OsDiskReadNext
    ret