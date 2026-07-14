#include "os_def.h"
#include "os_hwi.h"
#include "os_hwi_external.h"
#include "os_hwi_internal.h"
#include "os_idt_i386.h"
#include "os_io_i386.h"
#include "os_print_external.h"
#include "os_debug_external.h"
#include "os_syscall_i386.h"

/*
 * i386 硬件中断架构相关实现
 * - 8259A PIC 初始化
 * - IRQ 向量表
 * - 注册 IRQ 向量到共享 IDT,装载 IDTR
 */

OS_SEC_KERNEL_DATA OsHwiVector g_hwiVectorTab[OS_HWI_NUM] = {
    OS_HWI_VECTOR(0x20),
    OS_HWI_VECTOR(0x21),
    OS_HWI_VECTOR(0x22),
    OS_HWI_VECTOR(0x23),
    OS_HWI_VECTOR(0x24),
    OS_HWI_VECTOR(0x25),
    OS_HWI_VECTOR(0x26),
    OS_HWI_VECTOR(0x27),
    OS_HWI_VECTOR(0x28),
    OS_HWI_VECTOR(0x29),
    OS_HWI_VECTOR(0x2a),
    OS_HWI_VECTOR(0x2b),
    OS_HWI_VECTOR(0x2c),
    OS_HWI_VECTOR(0x2d),
    OS_HWI_VECTOR(0x2e),
    OS_HWI_VECTOR(0x2f),
    OS_HWI_VECTOR(0x30),
};

/* 8259A中断控制器初始化 */
OS_INLINE void OsHwiPicInit(void)
{
    OS_DEBUG_PRINT_STR("OsHwiPicInit begin\n");
    OsOutb(OS_PIC_M_CTRL, 0x11);
    OsOutb(OS_PIC_M_DATA, 0x20);
    OsOutb(OS_PIC_M_DATA, 0x04);
    OsOutb(OS_PIC_M_DATA, 0x01);
    OsOutb(OS_PIC_S_CTRL, 0x11);
    OsOutb(OS_PIC_S_DATA, 0x28);
    OsOutb(OS_PIC_S_DATA, 0x02);
    OsOutb(OS_PIC_S_DATA, 0x01);

    OsOutb(OS_PIC_M_DATA, 0xfe);
    OsOutb(OS_PIC_S_DATA, 0xff);
    OS_DEBUG_PRINT_STR("OsHwiPicInit end\n");
}

static OS_SEC_KERNEL_TEXT void OsHwiRegIdt(void)
{
    U32 i;
    for (i = OS_HWI_MIN; i <= OS_HWI_MAX; i++) {
        OsIdtBuildEntry(i, OS_IDT_ENTRY_ATTR0, g_hwiVectorTab[OsHwiNum2Idx(i)]);
        OsHwiCreate(i, OsHwiDefHandler);
    }
}

OS_SEC_KERNEL_TEXT U32 OsHwiConfigInit(void)
{
    OS_DEBUG_PRINT_STR("OsHwiConfig start\n");

    /* 异常向量由 OsExcConfigInit 先注册;此处只注册 IRQ 并装载 IDTR */
    OsHwiRegIdt();
    OsHwiPicInit();
    OsSyscallConfigInit();  /* 注册 INT 0x80 (DPL=3)，在 OsIdtLoad 之前 */
    OsIdtLoad();

    OS_DEBUG_PRINT_STR("OsHwiConfig end\n");
    return OS_OK;
}