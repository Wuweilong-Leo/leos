#ifndef OS_HWI_I386_H
#define OS_HWI_I386_H
#include "os_def.h"
#include "os_hwi_external.h"

/*
 * i386 硬件中断(IRQ)架构层:8259A PIC、IRQ 向量表、CPU 中断开关原语。
 * 异常处理见 os_exc_i386.h;IDT 共享层见 os_idt_i386.h。
 */

#define OS_HWI_MAX_NUM 0x31

#define OS_HWI_MIN 0X20
#define OS_HWI_MAX 0X30
#define OS_HWI_NUM (OS_HWI_MAX - OS_HWI_MIN + 1)

#define OS_PIC_M_CTRL 0x20
#define OS_PIC_M_DATA 0x21
#define OS_PIC_S_CTRL 0xa0
#define OS_PIC_S_DATA 0xa1

typedef void (*OsHwiVector)(void);

#define OS_HWI_VECTOR(hwiNum) (OsHwiVector##hwiNum)

/* 系统活跃标志位在 os_sys.h 中定义 */

/* 中断号到索引的转换（i386 中断号从 0x20 开始） */
OS_INLINE U32 OsHwiNum2Idx(U32 hwiNum)
{
    return hwiNum - OS_HWI_MIN;
}

/* CPU 中断开关原语(cli/sti/取 EFLAGS) */
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

extern void OS_HWI_VECTOR(0x20)(void);
extern void OS_HWI_VECTOR(0x21)(void);
extern void OS_HWI_VECTOR(0x22)(void);
extern void OS_HWI_VECTOR(0x23)(void);
extern void OS_HWI_VECTOR(0x24)(void);
extern void OS_HWI_VECTOR(0x25)(void);
extern void OS_HWI_VECTOR(0x26)(void);
extern void OS_HWI_VECTOR(0x27)(void);
extern void OS_HWI_VECTOR(0x28)(void);
extern void OS_HWI_VECTOR(0x29)(void);
extern void OS_HWI_VECTOR(0x2a)(void);
extern void OS_HWI_VECTOR(0x2b)(void);
extern void OS_HWI_VECTOR(0x2c)(void);
extern void OS_HWI_VECTOR(0x2d)(void);
extern void OS_HWI_VECTOR(0x2e)(void);
extern void OS_HWI_VECTOR(0x2f)(void);
extern void OS_HWI_VECTOR(0x30)(void);

extern U32 OsHwiConfigInit(void);

/* 中断默认处理函数（架构层注册用） */
extern void OsHwiDefHandler(U32 hwiNum);

#endif /* OS_HWI_I386_H */