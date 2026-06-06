#ifndef OS_HWI_I386_H
#define OS_HWI_I386_H
#include "os_def.h"
#include "os_hwi_external.h"
#include "os_context_i386.h"

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

typedef void (*OsHwiVector)(void);
typedef void (*OsExcVector)(void);

struct OsIdtEntry {
    U16 funcOffsetLowWord;
    U16 selector;
    U8 dcount;
    U8 attribute;
    U16 funcOffsetHighWord;
};

struct OsIdtInfo {
    U16 idtLmit;
    U32 idtBase;
} OS_STRUCT_PACKED;

/* 异常上下文在 os_context_i386.h 中定义 */

enum OsExcType {
    OS_EXC_TYPE_DIVIDE_ERROR = 0,
    OS_EXC_TYPE_DEBUG = 1,
    OS_EXC_TYPE_NMI = 2,
    OS_EXC_TYPE_BREAKPOINT = 3,
    OS_EXC_TYPE_OVERFLOW = 4,
    OS_EXC_TYPE_BOUND_RANGE = 5,
    OS_EXC_TYPE_INVALID_OPCODE = 6,
    OS_EXC_TYPE_DEVICE_NOT_AVAIL = 7,
    OS_EXC_TYPE_DOUBLE_FAULT = 8,
    OS_EXC_TYPE_COPROC_SEG_OVERRUN = 9,
    OS_EXC_TYPE_INVALID_TSS = 10,
    OS_EXC_TYPE_SEGMENT_NOT_PRESENT = 11,
    OS_EXC_TYPE_STACK_FAULT = 12,
    OS_EXC_TYPE_GPF = 13,
    OS_EXC_TYPE_PAGE_FAULT = 14,
    OS_EXC_TYPE_RESERVED = 15,
    OS_EXC_TYPE_FPU_ERROR = 16,
    OS_EXC_TYPE_ALIGNMENT_CHECK = 17,
    OS_EXC_TYPE_MACHINE_CHECK = 18,
    OS_EXC_TYPE_SIMD_FP = 19,
};

#define OS_IDT_ENTRY_ATTR_P       1
#define OS_IDT_ENTRY_ATTR_DPL0    0
#define OS_IDT_ENTRY_ATTR_DPL3    3
#define OS_IDT_ENTRY_ATTR_32_TYPE 0xE
#define OS_IDT_ENTRY_ATTR_16_TYPE 0x6

/* DPL为0 */
#define OS_IDT_ENTRY_ATTR0                                                                         \
    \                                                    
    ((OS_IDT_ENTRY_ATTR_P << 7) + (OS_IDT_ENTRY_ATTR_DPL0 << 5) + OS_IDT_ENTRY_ATTR_32_TYPE)
/* DPL为3 */
#define OS_IDT_ENTRY_ATTR3                                                                         \
    \                                                 
    ((OS_IDT_ENTRY_ATTR_P << 7) + (OS_IDT_ENTRY_ATTR_DPL3 << 5) + OS_IDT_ENTRY_ATTR_32_TYPE)

#define OS_HWI_VECTOR(hwiNum) (OsHwiVector##hwiNum)
#define OS_EXC_VECTOR(excNum) (OsExcVector##excNum)

#define OS_SELECTOR_K_CODE 0x08

/* 系统活跃标志位在 os_sys.h 中定义 */

/* 中断号到索引的转换（i386 中断号从 0x20 开始） */
OS_INLINE U32 OsHwiNum2Idx(U32 hwiNum)
{
    return hwiNum - OS_HWI_MIN;
}

OS_INLINE enum OsIntStatus OsGetIntStatus(void)
{
    U32 eflag;
    OS_EMBED_ASM("pushf; popl %0" : "=r"(eflag));
    return (eflag & 0x200) ? OS_INT_ON : OS_INT_OFF;
}

OS_INLINE enum OsIntStatus OsIntLock(void)
{
    enum OsIntStatus intSave = OsGetIntStatus();
    OS_EMBED_ASM("cli");
    return intSave;
}

OS_INLINE void OsIntRestore(enum OsIntStatus intSave)
{
    if (intSave == OS_INT_OFF) {
        OS_EMBED_ASM("cli");
    } else {
        OS_EMBED_ASM("sti");
    }
}

OS_INLINE enum OsIntStatus OsIntUnlock(void)
{
    enum OsIntStatus intSave = OsGetIntStatus();
    OS_EMBED_ASM("sti");
    return intSave;
}

extern void OS_EXC_VECTOR(0x00)(void);
extern void OS_EXC_VECTOR(0x01)(void);
extern void OS_EXC_VECTOR(0x02)(void);
extern void OS_EXC_VECTOR(0x03)(void);
extern void OS_EXC_VECTOR(0x04)(void);
extern void OS_EXC_VECTOR(0x05)(void);
extern void OS_EXC_VECTOR(0x06)(void);
extern void OS_EXC_VECTOR(0x07)(void);
extern void OS_EXC_VECTOR(0x08)(void);
extern void OS_EXC_VECTOR(0x09)(void);
extern void OS_EXC_VECTOR(0x0a)(void);
extern void OS_EXC_VECTOR(0x0b)(void);
extern void OS_EXC_VECTOR(0x0c)(void);
extern void OS_EXC_VECTOR(0x0d)(void);
extern void OS_EXC_VECTOR(0x0e)(void);
extern void OS_EXC_VECTOR(0x0f)(void);
extern void OS_EXC_VECTOR(0x10)(void);
extern void OS_EXC_VECTOR(0x11)(void);
extern void OS_EXC_VECTOR(0x12)(void);
extern void OS_EXC_VECTOR(0x13)(void);
extern void OS_EXC_VECTOR(0x14)(void);
extern void OS_EXC_VECTOR(0x15)(void);
extern void OS_EXC_VECTOR(0x16)(void);
extern void OS_EXC_VECTOR(0x17)(void);
extern void OS_EXC_VECTOR(0x18)(void);
extern void OS_EXC_VECTOR(0x19)(void);
extern void OS_EXC_VECTOR(0x1a)(void);
extern void OS_EXC_VECTOR(0x1b)(void);
extern void OS_EXC_VECTOR(0x1c)(void);
extern void OS_EXC_VECTOR(0x1d)(void);
extern void OS_EXC_VECTOR(0x1e)(void);
extern void OS_EXC_VECTOR(0x1f)(void);
extern void OS_HWI_VECTOR(0x20)(void);

extern U32 OsHwiConfigInit(void);
extern void OsExcDispatcher(U32 excNum, struct OsExcSaveContext *context);

/* IRQ 默认处理函数（架构层注册用） */
extern void OsHwiDefHandler(U32 irqNum);

#endif /* OS_HWI_I386_H */