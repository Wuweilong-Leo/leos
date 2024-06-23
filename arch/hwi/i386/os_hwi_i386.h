#ifndef OS_HWI_I386_H
#define OS_HWI_I386_H
#include "os_def.h"

#define OS_HWI_MAX_NUM 0x21
#define OS_EXC_MAX_NUM 20
#define OS_PIC_M_CTRL 0x20
#define OS_PIC_M_DATA 0x21
#define OS_PIC_S_CTRL 0xa0
#define OS_PIC_S_DATA 0xa1

typedef void (*OsHwiHandlerFunc)(U32 hwiNum, uintptr_t context);
typedef void (*OsHwiVector)(void);

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

enum OsIntStatus {
    OS_INT_OFF,
    OS_INT_ON
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

OS_INLINE enum OsIntStatus OsGetIntStatus(void)
{
    U32 eflag;

    OS_EMBED_ASM("pushf; popl %0" : "=r"(eflag));
    
    return (eflag & 0x200)? OS_INT_ON : OS_INT_OFF;
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

extern void OS_HWI_VECTOR(0x00) (void);
extern void OS_HWI_VECTOR(0x01) (void);
extern void OS_HWI_VECTOR(0x02) (void);
extern void OS_HWI_VECTOR(0x03) (void);
extern void OS_HWI_VECTOR(0x04) (void);
extern void OS_HWI_VECTOR(0x05) (void);
extern void OS_HWI_VECTOR(0x06) (void);
extern void OS_HWI_VECTOR(0x07) (void);
extern void OS_HWI_VECTOR(0x08) (void);
extern void OS_HWI_VECTOR(0x09) (void);
extern void OS_HWI_VECTOR(0x0a) (void);
extern void OS_HWI_VECTOR(0x0b) (void);
extern void OS_HWI_VECTOR(0x0c) (void);
extern void OS_HWI_VECTOR(0x0d) (void);
extern void OS_HWI_VECTOR(0x0e) (void);
extern void OS_HWI_VECTOR(0x0f) (void);
extern void OS_HWI_VECTOR(0x10) (void);
extern void OS_HWI_VECTOR(0x11) (void);
extern void OS_HWI_VECTOR(0x12) (void);
extern void OS_HWI_VECTOR(0x13) (void);
extern void OS_HWI_VECTOR(0x14) (void);
extern void OS_HWI_VECTOR(0x15) (void);
extern void OS_HWI_VECTOR(0x16) (void);
extern void OS_HWI_VECTOR(0x17) (void);
extern void OS_HWI_VECTOR(0x18) (void);
extern void OS_HWI_VECTOR(0x19) (void);
extern void OS_HWI_VECTOR(0x1a) (void);
extern void OS_HWI_VECTOR(0x1b) (void);
extern void OS_HWI_VECTOR(0x1c) (void);
extern void OS_HWI_VECTOR(0x1d) (void);
extern void OS_HWI_VECTOR(0x1e) (void);
extern void OS_HWI_VECTOR(0x1f) (void);
extern void OS_HWI_VECTOR(0x20) (void);

extern U32 OsHwiCreate(U32 hwiNum, OsHwiHandlerFunc isr);
extern void OsHwiConfig(void);

#endif