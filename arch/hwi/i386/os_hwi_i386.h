#ifndef OS_HWI_I386_H
#define OS_HWI_I386_H
#include "os_def.h"

#define OS_HWI_MAX_NUM 0x21
#define OS_EXC_MAX_NUM 20

#define OS_EXC_MIN 0
#define OS_EXC_MAX 0X1F
#define OS_EXC_NUM (OS_EXC_MAX - OS_EXC_MIN + 1)
#define OS_HWI_MIN 0X20
#define OS_HWI_MAX 0X20
#define OS_HWI_NUM (OS_HWI_MAX - OS_HWI_MIN + 1)

#define OS_PIC_M_CTRL 0x20
#define OS_PIC_M_DATA 0x21
#define OS_PIC_S_CTRL 0xa0
#define OS_PIC_S_DATA 0xa1

typedef void (*OsHwiHandlerFunc)(U32 hwiNum);
typedef void (*OsExcHandlerFunc)(U32 excNum, uintptr_t context);

typedef void (*OsHwiVector)(void);
typedef void (*OsExcVector)(void);

struct OsHwiForm {
    OsHwiHandlerFunc isr;
};

struct OsIdtEntry {
    U16 funcOffsetLowWord;
    U16 selector;
    U8 dcount;
    U8 attribute;
    U16 funcOffsetHighWord;
};

struct OS_STRUCT_PACKED OsIdtInfo {
    U16 idtLmit;
    U32 idtBase;
};

enum OsExcType {
    OS_EXC_TYPE_DIVIDE_ERROR = 0,      // 除法错误（除零）
    OS_EXC_TYPE_DEBUG = 1,             // 调试异常
    OS_EXC_TYPE_NMI = 2,               // 不可屏蔽中断
    OS_EXC_TYPE_BREAKPOINT = 3,        // 断点异常（INT3）
    OS_EXC_TYPE_OVERFLOW = 4,          // 溢出（INTO指令）
    OS_EXC_TYPE_BOUND_RANGE = 5,       // 边界检查异常（BOUND指令）
    OS_EXC_TYPE_INVALID_OPCODE = 6,    // 无效操作码
    OS_EXC_TYPE_DEVICE_NOT_AVAIL = 7,  // 设备不可用（FPU不存在）
    OS_EXC_TYPE_DOUBLE_FAULT = 8,      // 双重错误
    OS_EXC_TYPE_COPROC_SEG_OVERRUN = 9,// 协处理器段越界（保留）
    OS_EXC_TYPE_INVALID_TSS = 10,      // 无效TSS
    OS_EXC_TYPE_SEGMENT_NOT_PRESENT = 11, // 段不存在
    OS_EXC_TYPE_STACK_FAULT = 12,      // 栈异常
    OS_EXC_TYPE_GPF = 13,              // 通用保护错误
    OS_EXC_TYPE_PAGE_FAULT = 14,       // 页错误
    OS_EXC_TYPE_RESERVED = 15,         // Intel保留
    OS_EXC_TYPE_FPU_ERROR = 16,        // FPU浮点错误
    OS_EXC_TYPE_ALIGNMENT_CHECK = 17,  // 对齐检查（486+）
    OS_EXC_TYPE_MACHINE_CHECK = 18,    // 机器检查（Pentium+）
    OS_EXC_TYPE_SIMD_FP = 19,          // SIMD浮点异常（Pentium III+）
};


#define OS_IDT_ENTRY_ATTR_P 1
#define OS_IDT_ENTRY_ATTR_DPL0 0
#define OS_IDT_ENTRY_ATTR_DPL3 3
#define OS_IDT_ENTRY_ATTR_32_TYPE 0xE
#define OS_IDT_ENTRY_ATTR_16_TYPE 0x6

/* DPL为0 */
#define OS_IDT_ENTRY_ATTR0 \                                                    
    ((OS_IDT_ENTRY_ATTR_P << 7) + (OS_IDT_ENTRY_ATTR_DPL0 << 5) + OS_IDT_ENTRY_ATTR_32_TYPE)
/* DPL为3 */
#define OS_IDT_ENTRY_ATTR3 \                                                 
    ((OS_IDT_ENTRY_ATTR_P << 7) + (OS_IDT_ENTRY_ATTR_DPL3 << 5) + OS_IDT_ENTRY_ATTR_32_TYPE)

#define OS_HWI_VECTOR(hwiNum) \
    (OsHwiVector##hwiNum)

#define OS_EXC_VECTOR(excNum) \
    (OsExcVector##excNum)

OS_INLINE enum OsIntStatus OsGetIntStatus(void)
{
    U32 eflag;

    OS_EMBED_ASM("pushf; popl %0" : "=r"(eflag));
    
    return (eflag & 0x200)? OS_INT_ON : OS_INT_OFF;
}

// 只能在内核态使用
OS_INLINE enum OsIntStatus OsIntLock(void)
{
    enum OsIntStatus intSave = OsGetIntStatus();

    OS_EMBED_ASM("cli");

    return intSave;
}

// 只能在内核态使用
OS_INLINE void OsIntRestore(enum OsIntStatus intSave)
{
    if (intSave == OS_INT_OFF) {
        OS_EMBED_ASM("cli");
    } else {
        OS_EMBED_ASM("sti");
    }
}

// 只能在内核态使用
OS_INLINE enum OsIntStatus OsIntUnlock(void)
{
    enum OsIntStatus intSave = OsGetIntStatus();

    OS_EMBED_ASM("sti");

    return intSave;
}

extern void OS_EXC_VECTOR(0x00) (void);
extern void OS_EXC_VECTOR(0x01) (void);
extern void OS_EXC_VECTOR(0x02) (void);
extern void OS_EXC_VECTOR(0x03) (void);
extern void OS_EXC_VECTOR(0x04) (void);
extern void OS_EXC_VECTOR(0x05) (void);
extern void OS_EXC_VECTOR(0x06) (void);
extern void OS_EXC_VECTOR(0x07) (void);
extern void OS_EXC_VECTOR(0x08) (void);
extern void OS_EXC_VECTOR(0x09) (void);
extern void OS_EXC_VECTOR(0x0a) (void);
extern void OS_EXC_VECTOR(0x0b) (void);
extern void OS_EXC_VECTOR(0x0c) (void);
extern void OS_EXC_VECTOR(0x0d) (void);
extern void OS_EXC_VECTOR(0x0e) (void);
extern void OS_EXC_VECTOR(0x0f) (void);
extern void OS_EXC_VECTOR(0x10) (void);
extern void OS_EXC_VECTOR(0x11) (void);
extern void OS_EXC_VECTOR(0x12) (void);
extern void OS_EXC_VECTOR(0x13) (void);
extern void OS_EXC_VECTOR(0x14) (void);
extern void OS_EXC_VECTOR(0x15) (void);
extern void OS_EXC_VECTOR(0x16) (void);
extern void OS_EXC_VECTOR(0x17) (void);
extern void OS_EXC_VECTOR(0x18) (void);
extern void OS_EXC_VECTOR(0x19) (void);
extern void OS_EXC_VECTOR(0x1a) (void);
extern void OS_EXC_VECTOR(0x1b) (void);
extern void OS_EXC_VECTOR(0x1c) (void);
extern void OS_EXC_VECTOR(0x1d) (void);
extern void OS_EXC_VECTOR(0x1e) (void);
extern void OS_EXC_VECTOR(0x1f) (void);
extern void OS_HWI_VECTOR(0x20) (void);

extern U32 OsHwiCreate(U32 hwiNum, OsHwiHandlerFunc isr);
extern U32 OsHwiConfigInit(void);

#endif