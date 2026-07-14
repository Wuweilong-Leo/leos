#ifndef OS_IDT_I386_H
#define OS_IDT_I386_H
#include "os_def.h"

/*
 * i386 IDT 共享层:exc 与 hwi 都是 IDT 的客户端,谁都不"拥有"它。
 * - g_idt[] / g_idtInfo 为本模块私有,外部只通过 OsIdtBuildEntry 按向量号注册入口。
 * - OsIdtLoad 装载 IDTR(lidt),由初始化编排里负责整体装载的一方调用一次。
 */

/* IDT 完整 256 项（i386 标准），覆盖 exc + hwi + syscall */
#define OS_IDT_VEC_MAX  0xFF
#define OS_IDT_NUM      (OS_IDT_VEC_MAX + 1)

/* IDT 门描述符 */
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

/* 统一的中断/异常入口函数类型(exc 的 OsExcVector 与 hwi 的 OsHwiVector 同为 void(*)(void)) */
typedef void (*OsIdtVector)(void);

/* IDT 门属性 */
#define OS_IDT_ENTRY_ATTR_P       1
#define OS_IDT_ENTRY_ATTR_DPL0    0
#define OS_IDT_ENTRY_ATTR_DPL3    3
#define OS_IDT_ENTRY_ATTR_32_TYPE 0xE
#define OS_IDT_ENTRY_ATTR_16_TYPE 0x6

/* DPL为0 */
#define OS_IDT_ENTRY_ATTR0 ((OS_IDT_ENTRY_ATTR_P << 7) + (OS_IDT_ENTRY_ATTR_DPL0 << 5) + OS_IDT_ENTRY_ATTR_32_TYPE)
/* DPL为3 */
#define OS_IDT_ENTRY_ATTR3 ((OS_IDT_ENTRY_ATTR_P << 7) + (OS_IDT_ENTRY_ATTR_DPL3 << 5) + OS_IDT_ENTRY_ATTR_32_TYPE)

/* 按向量号填充一个 IDT 门 */
extern void OsIdtBuildEntry(U32 vecNum, U8 attr, OsIdtVector vecFunc);

/* 装载 IDTR(lidt) */
extern void OsIdtLoad(void);

#endif /* OS_IDT_I386_H */