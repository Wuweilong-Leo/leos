#include "os_def.h"
#include "os_idt_i386.h"
#include "os_cpu_i386.h" /* OS_SELECTOR_K_CODE */

/*
 * i386 IDT 共享实现:g_idt / g_idtInfo 私有,exc 与 hwi 通过 OsIdtBuildEntry
 * 按向量号注册各自入口,OsIdtLoad 一次性装载 IDTR。
 */

OS_SEC_KERNEL_DATA struct OsIdtEntry g_idt[OS_IDT_NUM];
OS_SEC_KERNEL_DATA struct OsIdtInfo g_idtInfo = {.idtLmit = sizeof(g_idt) - 1,
                                                 .idtBase = (U32)g_idt};

OS_SEC_KERNEL_TEXT void OsIdtBuildEntry(U32 vecNum, U8 attr, OsIdtVector vecFunc)
{
    struct OsIdtEntry *entry = &g_idt[vecNum];
    entry->funcOffsetLowWord = (U32)vecFunc & 0xFFFF;
    entry->selector = OS_SELECTOR_K_CODE;
    entry->attribute = attr;
    entry->dcount = 0;
    entry->funcOffsetHighWord = ((U32)vecFunc >> 16) & 0xFFFF;
}

OS_SEC_KERNEL_TEXT void OsIdtLoad(void)
{
    OS_EMBED_ASM("lidt %0" ::"m"(g_idtInfo) :);
}