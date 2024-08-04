#ifndef OS_TSS_H
#define OS_TSS_H
#include "os_def.h"
#include "os_cpu_i386.h"

struct OsTss {
    U32 backlink;
    U32 *esp0;
    U32 ss0;
    U32 *esp1;
    U32 ss1;
    U32 *esp2;
    U32 ss2;
    U32 cr3;
    U32 (*eip) (void);
    U32 eflags;
    U32 eax;
    U32 ecx;
    U32 edx;
    U32 ebx;
    U32 esp;
    U32 ebp;
    U32 esi;
    U32 edi;
    U32 es;
    U32 cs;
    U32 ss;
    U32 ds;
    U32 fs;
    U32 gs;
    U32 ldt;
    U32 trace;
    U32 ioBase;
};

#define OS_GDT_TSS_IDX 0x4
/* 应该是手册上 “可用的80386TSS” 类型 */
#define OS_GDT_TSS_ATTR_TYPE (U8)0b1001

#define OS_SELECTOR_TSS ((OS_GDT_TSS_IDX << 3) + (OS_TI_GDT << 2) + OS_RPL0)

extern void OsTssConfig(void);

OS_INLINE void OsLoadTss(void)
{
    OS_EMBED_ASM("ltr %w0"::"r"(OS_SELECTOR_TSS));
}
#endif