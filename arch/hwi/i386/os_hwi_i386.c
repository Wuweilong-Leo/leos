#include "os_def.h"
#include "os_hwi.h"
#include "os_sched_external.h"
#include "os_io_i386.h"
#include "os_cpu.h"
#include "os_print_external.h"
#include "os_debug_external.h"
#include "os_task_external.h"
#include "os_context_i386.h"
#include "os_sys.h"
#include "os_tick_external.h"

/*
 * i386中断异常都根据IDT，走一个流程
 * 前20个中断号其实是异常号
 */
OS_SEC_KERNEL_DATA struct OsHwiForm g_hwiForm[OS_HWI_NUM];
OS_SEC_KERNEL_DATA struct OsIdtEntry g_idt[OS_EXC_NUM + OS_HWI_NUM];
OS_SEC_KERNEL_DATA OsExcVector g_excVectorTab[OS_EXC_NUM] = {
    OS_EXC_VECTOR(0x00),
    OS_EXC_VECTOR(0x01),
    OS_EXC_VECTOR(0x02),
    OS_EXC_VECTOR(0x03),
    OS_EXC_VECTOR(0x04),
    OS_EXC_VECTOR(0x05),
    OS_EXC_VECTOR(0x06),
    OS_EXC_VECTOR(0x07),
    OS_EXC_VECTOR(0x08),
    OS_EXC_VECTOR(0x09),
    OS_EXC_VECTOR(0x0a),
    OS_EXC_VECTOR(0x0b),
    OS_EXC_VECTOR(0x0c),
    OS_EXC_VECTOR(0x0d),
    OS_EXC_VECTOR(0x0e),
    OS_EXC_VECTOR(0x0f),
    OS_EXC_VECTOR(0x10),
    OS_EXC_VECTOR(0x11),
    OS_EXC_VECTOR(0x12),
    OS_EXC_VECTOR(0x13),
    OS_EXC_VECTOR(0x14),
    OS_EXC_VECTOR(0x15),
    OS_EXC_VECTOR(0x16),
    OS_EXC_VECTOR(0x17),
    OS_EXC_VECTOR(0x18),
    OS_EXC_VECTOR(0x19),
    OS_EXC_VECTOR(0x1a),
    OS_EXC_VECTOR(0x1b),
    OS_EXC_VECTOR(0x1c),
    OS_EXC_VECTOR(0x1d),
    OS_EXC_VECTOR(0x1e),
    OS_EXC_VECTOR(0x1f),
};

OS_SEC_KERNEL_DATA OsHwiVector g_hwiVectorTab[OS_HWI_NUM] = {
    OS_HWI_VECTOR(0x20),
};

OS_SEC_KERNEL_DATA char *g_excNameTab[OS_EXC_NUM] = {
    [OS_EXC_TYPE_DIVIDE_ERROR] = "DEVIDE ZERO EXC",
    [OS_EXC_TYPE_DEBUG] = "DEBUG EXC",
    [OS_EXC_TYPE_NMI] = "NMI",
    [OS_EXC_TYPE_BREAKPOINT] = "BREAK POINT EXC",
    [OS_EXC_TYPE_OVERFLOW] = "OVERFLOW EXC",
    [OS_EXC_TYPE_BOUND_RANGE] = "BOUND RANGE EXCEEDED EXC",
    [OS_EXC_TYPE_INVALID_OPCODE] = "INVALID OPCODE EXC",
    [OS_EXC_TYPE_DEVICE_NOT_AVAIL] = "DEVICE NOT AVAILABLE EXC",
    [OS_EXC_TYPE_DOUBLE_FAULT] = "DOUBLE FAULT EXC",
    [OS_EXC_TYPE_COPROC_SEG_OVERRUN] = "COPROCESSOR SEGMENT OVERRUN",
    [OS_EXC_TYPE_INVALID_TSS] = "INVALID TSS EXC",
    [OS_EXC_TYPE_SEGMENT_NOT_PRESENT] = "SEGMENT NOT PRESENT",
    [OS_EXC_TYPE_STACK_FAULT] = "STACK FAULT EXC",
    [OS_EXC_TYPE_GPF] = "GENERAL PROTECTION EXC",
    [OS_EXC_TYPE_PAGE_FAULT] = "PAGE FAULT EXC",
    [OS_EXC_TYPE_RESERVED] = "INTEL RESERVE", // 15为intel保留项，未使用
    [OS_EXC_TYPE_FPU_ERROR] = "FPU FLOATING POINT ERR",
    [OS_EXC_TYPE_ALIGNMENT_CHECK] = "ALIGNMENT CHECK EXC",
    [OS_EXC_TYPE_MACHINE_CHECK] = "MACHINE CHECK EXC",
    [OS_EXC_TYPE_SIMD_FP] = "SIMD FLOATING POINT EXC",
    [OS_EXC_TYPE_SIMD_FP + 1 ... OS_EXC_MAX] = NULL
};

OS_SEC_KERNEL_DATA struct OsIdtInfo g_idtInfo = {
    .idtLmit = sizeof(g_idt) - 1,
    .idtBase = (U32)g_idt
};

OS_INLINE U32 OsExcNum2Idx(U32 excNum)
{
    return excNum - OS_EXC_MIN;
}

static OS_SEC_KERNEL_TEXT void OsHwiDefHandler(U32 hwiNum)
{
    (void)hwiNum;
    return;
}

OS_INLINE U32 OsHwiNum2Idx(U32 hwiNum)
{
    return hwiNum - OS_HWI_MIN;
}

OS_SEC_KERNEL_TEXT U32 OsHwiCreate(U32 hwiNum, OsHwiHandlerFunc isr)
{
    g_hwiForm[OsHwiNum2Idx(hwiNum)].isr = isr;
    return OS_OK;
}

OS_SEC_KERNEL_TEXT void OsHwiDispatcher(U32 hwiNum)
{
    struct OsRunQue *rq = OS_RUN_QUE();
    OsHwiHandlerFunc isr = g_hwiForm[OsHwiNum2Idx(hwiNum)].isr;
    
    rq->intCount++;
    rq->uniFlag |= OS_HWI_ACTIVE_MSK;
    isr(hwiNum);
    rq->uniFlag &= ~OS_HWI_ACTIVE_MSK;
    rq->intCount--;
}

OS_SEC_KERNEL_TEXT void OsExcReport(U32 excNum, struct OsExcSaveContext *context)
{
    char *excName = g_excNameTab[OsExcNum2Idx(excNum)];
    if (excName != NULL) {
        OS_DEBUG_KPRINT("exc num: 0x%x, exc type: %s, exc addr: 0x%x, exc cs: 0x%x, exc pc 0x%x, \
                eax: 0x%x, ebx: 0x%x, ecx: 0x%x, edx: 0x%x\n",
                excNum, excName, context->cr2, context->cs, context->eip, 
                context->eax, context->ebx, context->ecx, context->edx);
    } else {
        OS_DEBUG_KPRINT("unknown exc type!!!\n");
    }

    while (1) {}
}

OS_INLINE bool OsExcPgFaultTriggeredByKernel(U32 errCode)
{
    return (errCode & 0x4) == 0;
}

OS_SEC_KERNEL_TEXT bool OsExcHandleKernelPgFault(uintptr_t errAddr)
{
    uintptr_t pgBase;

    if (errAddr >= OS_KERNEL_VIR_HEAP_MEM_BASE && errAddr < OS_KERNEL_VIR_HEAP_MEM_BASE + OS_KERNEL_VIR_HEAP_MEM_SIZE) {
        // errAddr那一页并未映射
        pgBase = OS_ROUND_DOWN(errAddr, OS_PG_SIZE);
        return OsMemKernelAllocPgByAddr(pgBase) != NULL;
    } else {
        OS_DEBUG_KPRINT("OsExcHandleKernelPgFault: errAddr not in range, 0x%x\n", (U32)errAddr);
        return FALSE;
    }
}

OS_SEC_KERNEL_TEXT void OsExcDispatcher(U32 excNum, struct OsExcSaveContext *context)
{
    if (excNum > OS_EXC_MAX) {
        OS_LOG_ERROR("OsExcDispatcher: excNum 0x%x out of range (max 0x%x)\n", excNum, OS_EXC_MAX);
        while (1) {}
    }

    if (excNum == OS_EXC_TYPE_PAGE_FAULT && 
        OsExcPgFaultTriggeredByKernel(context->errCode)) {
        if (!OsExcHandleKernelPgFault(context->cr2)) {
            OS_DEBUG_KPRINT("cs:0x%x, eip:0x%x,errAddr:0x%x\n", context->cs, context->eip, context->cr2);
            while (1) {}
        }
    } else {
        OsExcReport(excNum, context);
        while (1) {}
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

static OS_SEC_KERNEL_TEXT void OsBuildIdtEntry(struct OsIdtEntry *entry,
                                                  U8 attr, OsHwiVector vecFunc)
{
    entry->funcOffsetLowWord = (U32)vecFunc & 0xFFFF;
    entry->selector = OS_SELECTOR_K_CODE;
    entry->attribute = attr;
    entry->dcount = 0;
    entry->funcOffsetHighWord = ((U32)vecFunc >> 16) & 0xFFFF;
}

static OS_SEC_KERNEL_TEXT void OsExcRegIdt(void)
{
    U32 i;

    // 注册异常的统一钩子
    for (i = OS_EXC_MIN; i <= OS_EXC_MAX; i++) {
        OsBuildIdtEntry(&g_idt[i], OS_IDT_ENTRY_ATTR0, g_excVectorTab[OsExcNum2Idx(i)]);
    }
}


static OS_SEC_KERNEL_TEXT void OsHwiRegIdt(void)
{
    U32 i;

    // 注册异常的统一钩子
    for (i = OS_HWI_MIN; i <= OS_HWI_MAX; i++) {
        OsBuildIdtEntry(&g_idt[i], OS_IDT_ENTRY_ATTR0, g_hwiVectorTab[OsHwiNum2Idx(i)]);
        OsHwiCreate(i, OsHwiDefHandler);
    }
}

OS_SEC_KERNEL_TEXT U32 OsHwiConfigInit(void)
{
    OS_DEBUG_PRINT_STR("OsHwiConfig start\n");

    OsExcRegIdt();
    OsHwiRegIdt();

    OsHwiPicInit();

    OS_EMBED_ASM("lidt %0"::"m"(g_idtInfo):);

    OS_DEBUG_PRINT_STR("OsHwiConfig end\n");
    return OS_OK;
}

// 中断尾部
OS_SEC_KERNEL_TEXT void OsHwiTail(void)
{
    // 中断尾部处理ticks
    struct OsRunQue *rq = OS_RUN_QUE();
    enum OsIntStatus intSave;

    if (UNLIKELY(g_noRespondTicks > 0)) {
        if (OS_TICK_ACTIVE(rq->uniFlag)) {
            return;
        }
        rq->uniFlag |= OS_TICK_ACTIVE_MSK;
        do {
            intSave = OsIntUnlock();
            OsTickDispatcher();
            OsIntRestore(intSave);
            g_noRespondTicks--;
        } while (g_noRespondTicks > 0);
        rq->uniFlag &= ~OS_TICK_ACTIVE_MSK;
    }

    OsSchedMain();
}


