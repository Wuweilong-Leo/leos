#include "os_def.h"
#include "os_hwi_i386.h"
#include "os_sched_external.h"
#include "os_io_i386.h"
#include "os_cpu_i386.h"
#include "os_print_external.h"
#include "os_debug_external.h"

/*
 * i386中断异常都根据IDT，走一个流程
 * 前20个中断号其实是异常号
 */
OS_SEC_KERNEL_DATA struct OsHwiForm g_hwiForm[OS_HWI_MAX_NUM];
OS_SEC_KERNEL_DATA struct OsIdtEntry g_idt[OS_HWI_MAX_NUM];
OS_SEC_KERNEL_DATA OsHwiVector g_hwiVectorTab[OS_HWI_MAX_NUM] = {
    OS_HWI_VECTOR(0x00),
    OS_HWI_VECTOR(0x01),
    OS_HWI_VECTOR(0x02),
    OS_HWI_VECTOR(0x03),
    OS_HWI_VECTOR(0x04),
    OS_HWI_VECTOR(0x05),
    OS_HWI_VECTOR(0x06),
    OS_HWI_VECTOR(0x07),
    OS_HWI_VECTOR(0x08),
    OS_HWI_VECTOR(0x09),
    OS_HWI_VECTOR(0x0a),
    OS_HWI_VECTOR(0x0b),
    OS_HWI_VECTOR(0x0c),
    OS_HWI_VECTOR(0x0d),
    OS_HWI_VECTOR(0x0e),
    OS_HWI_VECTOR(0x0f),
    OS_HWI_VECTOR(0x10),
    OS_HWI_VECTOR(0x11),
    OS_HWI_VECTOR(0x12),
    OS_HWI_VECTOR(0x13),
    OS_HWI_VECTOR(0x14),
    OS_HWI_VECTOR(0x15),
    OS_HWI_VECTOR(0x16),
    OS_HWI_VECTOR(0x17),
    OS_HWI_VECTOR(0x18),
    OS_HWI_VECTOR(0x19),
    OS_HWI_VECTOR(0x1a),
    OS_HWI_VECTOR(0x1b),
    OS_HWI_VECTOR(0x1c),
    OS_HWI_VECTOR(0x1d),
    OS_HWI_VECTOR(0x1e),
    OS_HWI_VECTOR(0x1f),
    OS_HWI_VECTOR(0x20),
};

OS_SEC_KERNEL_DATA char *g_excNameTab[OS_EXC_MAX_NUM] = {
    "DEVIDE ZERO EXC",
    "DEBUG EXC",
    "NMI",
    "BREAK POINT EXC",
    "OVERFLOW EXC",
    "BOUND RANGE EXCEEDED EXC",
    "INVALID OPCODE EXC",
    "DEVICE NOT AVAILABLE EXC"
    "DOUBLE FAULT EXC",
    "COPROCESSOR SEGMENT OVERRUN",
    "INVALID TSS EXC",
    "SEGMENT NOT PRESENT",
    "STACK FAULT EXC",
    "GENERAL PROTECTION EXC",
    "PAGE FAULT EXC",
    NULL, // 15为intel保留项，未使用
    "FPU FLOATING POINT ERR",
    "ALIGNMENT CHECK EXC",
    "MACHINE CHECK EXC",
    "SIMD FLOATING POINT EXC"
};

OS_SEC_KERNEL_DATA struct OsIdtInfo g_idtInfo = {
    .idtLmit = sizeof(g_idt) - 1,
    .idtBase = g_idt
};

static OS_SEC_KERNEL_TEXT void OsHwiDefHandler(U32 hwiNum)
{
    (void)hwiNum;
    return;
}

OS_SEC_KERNEL_TEXT U32 OsHwiCreate(U32 hwiNum, OsHwiHandlerFunc isr)
{
    g_hwiForm[hwiNum].isr = isr;
    return OS_OK;
}

OS_SEC_KERNEL_TEXT void OsHwiDispatcher(U32 hwiNum)
{
    OsHwiHandlerFunc isr;
    g_runQue.intCount++;
    isr = g_hwiForm[hwiNum].isr;
    isr(hwiNum);
    g_runQue.intCount--;
}

static OS_SEC_KERNEL_TEXT void OsExcDefHandler(U32 excNum)
{
    OS_ASSERT(excNum < OS_EXC_MAX_NUM);

    OsPrintStr(g_excNameTab[excNum]);

    while (1) {}
}

static OS_SEC_KERNEL_TEXT void OsExcConfig(void)
{
    U32 i;

    for (i = 0; i < OS_EXC_MAX_NUM; i++) {
        g_hwiForm[i].isr = OsExcDefHandler;
    }
}

/* 8259A中断控制器初始化 */
OS_INLINE void OsHwiPicInit(void)
{
    OS_DEBUG_PRINT_STR("OsHwiPicInit begin\n");
    /* 初始化主片 */
    OsOutb(OS_PIC_M_CTRL, 0x11);
    OsOutb(OS_PIC_M_DATA, 0x20);
    OsOutb(OS_PIC_M_DATA, 0x04);
    OsOutb(OS_PIC_M_DATA, 0x01);
    /* 初始化从片 */
    OsOutb(OS_PIC_S_CTRL, 0x11);
    OsOutb(OS_PIC_S_DATA, 0x28);
    OsOutb(OS_PIC_S_DATA, 0x02);
    OsOutb(OS_PIC_S_DATA, 0x01);

    OsOutb(OS_PIC_M_DATA, 0xfe);
    OsOutb(OS_PIC_S_DATA, 0xff);
    OS_DEBUG_PRINT_STR("OsHwiPicInit end\n");

}

static OS_SEC_KERNEL_TEXT void OsHwiBuildIdtEntry(struct OsIdtEntry *entry,
                                                  U8 attr, OsHwiVector vecFunc)
{
    entry->funcOffsetLowWord = (U32)vecFunc & 0xFFFF;
    entry->selector = OS_SELECTOR_K_CODE;
    entry->attribute = attr;
    entry->dcount = 0;
    entry->funcOffsetHighWord = ((U32)vecFunc >> 16) & 0xFFFF;
}

OS_SEC_KERNEL_TEXT void OsHwiConfig(void)
{
    U32 i;

    OS_DEBUG_PRINT_STR("OsHwiConfig start\n");

    for (i = 0; i < OS_HWI_MAX_NUM; i++) {
        OsHwiBuildIdtEntry(&g_idt[i], OS_IDT_ENTRY_ATTR0, g_hwiVectorTab[i]);
        g_hwiForm[i].isr = OsHwiDefHandler;
    }

    OsExcConfig();

    OsHwiPicInit();

    OS_EMBED_ASM("lidt %0"::"m"(g_idtInfo):);

    OS_DEBUG_PRINT_STR("OsHwiConfig end\n");
}



