#ifndef OS_EXC_I386_H
#define OS_EXC_I386_H
#include "os_def.h"
#include "os_context_i386.h" /* struct OsExcSaveContext */

/*
 * i386 异常(CPU exception)架构层:异常向量表、异常名、异常分发。
 * 与 hwi 独立;IDT 注册经共享层 os_idt_i386.h。
 */

#define OS_EXC_MAX_NUM 20
#define OS_EXC_MIN 0
#define OS_EXC_MAX 0X1F
#define OS_EXC_NUM (OS_EXC_MAX - OS_EXC_MIN + 1)

typedef void (*OsExcVector)(void);

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

#define OS_EXC_VECTOR(excNum) (OsExcVector##excNum)

/* 异常号到索引的转换 */
OS_INLINE U32 OsExcNum2Idx(U32 excNum)
{
    return excNum - OS_EXC_MIN;
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

/* 异常分发(由架构层异常入口 os_dispatch.S 调用) */
extern void OsExcDispatcher(U32 excNum, struct OsExcSaveContext *context);

/* 异常模块初始化:向 IDT 注册异常向量 */
extern U32 OsExcConfigInit(void);

#endif /* OS_EXC_I386_H */