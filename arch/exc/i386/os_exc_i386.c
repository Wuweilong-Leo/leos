#include "os_def.h"
#include "os_exc.h"
#include "os_idt_i386.h"
#include "os_print_external.h"
#include "os_debug_external.h"
#include "os_mem_external.h"

/*
 * i386 异常架构相关实现
 * - 异常向量表(经 os_dispatch.S 的 OS_EXC_VECTOR 宏生成入口)
 * - 异常名表
 * - 异常分发(缺页按需映射 / 其余报告并挂死)
 * - 注册异常向量到共享 IDT
 */

OS_SEC_KERNEL_DATA OsExcVector g_excVectorTab[OS_EXC_NUM] = {
    OS_EXC_VECTOR(0x00), OS_EXC_VECTOR(0x01), OS_EXC_VECTOR(0x02), OS_EXC_VECTOR(0x03),
    OS_EXC_VECTOR(0x04), OS_EXC_VECTOR(0x05), OS_EXC_VECTOR(0x06), OS_EXC_VECTOR(0x07),
    OS_EXC_VECTOR(0x08), OS_EXC_VECTOR(0x09), OS_EXC_VECTOR(0x0a), OS_EXC_VECTOR(0x0b),
    OS_EXC_VECTOR(0x0c), OS_EXC_VECTOR(0x0d), OS_EXC_VECTOR(0x0e), OS_EXC_VECTOR(0x0f),
    OS_EXC_VECTOR(0x10), OS_EXC_VECTOR(0x11), OS_EXC_VECTOR(0x12), OS_EXC_VECTOR(0x13),
    OS_EXC_VECTOR(0x14), OS_EXC_VECTOR(0x15), OS_EXC_VECTOR(0x16), OS_EXC_VECTOR(0x17),
    OS_EXC_VECTOR(0x18), OS_EXC_VECTOR(0x19), OS_EXC_VECTOR(0x1a), OS_EXC_VECTOR(0x1b),
    OS_EXC_VECTOR(0x1c), OS_EXC_VECTOR(0x1d), OS_EXC_VECTOR(0x1e), OS_EXC_VECTOR(0x1f),
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
    [OS_EXC_TYPE_RESERVED] = "INTEL RESERVE",
    [OS_EXC_TYPE_FPU_ERROR] = "FPU FLOATING POINT ERR",
    [OS_EXC_TYPE_ALIGNMENT_CHECK] = "ALIGNMENT CHECK EXC",
    [OS_EXC_TYPE_MACHINE_CHECK] = "MACHINE CHECK EXC",
    [OS_EXC_TYPE_SIMD_FP] = "SIMD FLOATING POINT EXC",
    [OS_EXC_TYPE_SIMD_FP + 1 ... OS_EXC_MAX] = NULL};

OS_SEC_KERNEL_TEXT void OsExcReport(U32 excNum, struct OsExcSaveContext *context)
{
    char *excName = g_excNameTab[OsExcNum2Idx(excNum)];
    if (excName != NULL) {
        kprintf("\n!!! EXCEPTION: 0x%x %s !!!\n", excNum, excName);
        kprintf("  cs=0x%x eip=0x%x errCode=0x%x cr2=0x%x\n",
                context->cs, context->eip, context->errCode, context->cr2);
        kprintf("  eax=0x%x ebx=0x%x ecx=0x%x edx=0x%x\n",
                context->eax, context->ebx, context->ecx, context->edx);
    } else {
        kprintf("\n!!! UNKNOWN EXCEPTION: 0x%x !!!\n", excNum);
    }

    while (1) {
    }
}

OS_INLINE bool OsExcPgFaultTriggeredByKernel(U32 errCode)
{
    return (errCode & 0x4) == 0;
}

OS_SEC_KERNEL_TEXT bool OsExcHandleKernelPgFault(uintptr_t errAddr)
{
    uintptr_t pgBase;

    if (errAddr >= OS_KERNEL_VIR_HEAP_MEM_BASE &&
        errAddr < OS_KERNEL_VIR_HEAP_MEM_BASE + OS_KERNEL_VIR_HEAP_MEM_SIZE) {
        pgBase = OS_ROUND_DOWN(errAddr, OS_PG_SIZE);
        return OsMemKernelAllocPgByAddr(pgBase) != NULL;
    } else {
        OS_DEBUG_KPRINT("OsExcHandleKernelPgFault: errAddr not in range, 0x%x\n", (uintptr_t)errAddr);
        return FALSE;
    }
}

OS_SEC_KERNEL_TEXT void OsExcDispatcher(U32 excNum, struct OsExcSaveContext *context)
{
    if (excNum > OS_EXC_MAX) {
        kprintf("\n!!! EXCEPTION OUT OF RANGE: 0x%x !!!\n", excNum);
        while (1) {
        }
    }

    if (excNum == OS_EXC_TYPE_PAGE_FAULT && OsExcPgFaultTriggeredByKernel(context->errCode)) {
        if (!OsExcHandleKernelPgFault(context->cr2)) {
            kprintf("\n!!! KERNEL PAGE FAULT: cs=0x%x eip=0x%x errAddr=0x%x !!!\n",
                    context->cs, context->eip, context->cr2);
            while (1) {
            }
        }
    } else {
        OsExcReport(excNum, context);
    }
}

static OS_SEC_KERNEL_TEXT void OsExcRegIdt(void)
{
    U32 i;
    for (i = OS_EXC_MIN; i <= OS_EXC_MAX; i++) {
        OsIdtBuildEntry(i, OS_IDT_ENTRY_ATTR0, g_excVectorTab[OsExcNum2Idx(i)]);
    }
}

OS_SEC_KERNEL_TEXT U32 OsExcConfigInit(void)
{
    OS_DEBUG_PRINT_STR("OsExcConfig start\n");
    OsExcRegIdt();
    OS_DEBUG_PRINT_STR("OsExcConfig end\n");
    return OS_OK;
}