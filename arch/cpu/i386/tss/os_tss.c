#include "os_tss.h"
#include "os_def.h"
#include "os_gdt.h"

OS_SEC_KERNEL_BSS struct OsTss g_tss;

OS_SEC_KERNEL_TEXT void OsTssConfig(void)
{
    struct OsTss *tss = &g_tss;

    /* 创建tss的描述符 */
    OsBuildGdtEntry(OS_GDT_TSS_IDX, (uintptr_t)tss, sizeof(struct OsTss) - 1, OS_GDT_TSS_ATTR_TYPE,
                    OS_GDT_ENTRY_ATTR_S_SYS, OS_GDT_ENTRY_ATTR_DPL_0, OS_GDT_ENTRY_ATTR_P, 
                    OS_GDT_ENTRY_ATTR_AVL, OS_GDT_ENTRY_ATTR_L_0, OS_GDT_ENTRY_ATTR_DB_0, OS_GDT_ENTRY_ATTR_G_4K);    
}

/* 专门用来更新内核栈 */
OS_SEC_KERNEL_TEXT void OsTssUpdateEsp0(U32 ss0, uintptr_t esp0)
{
    g_tss.esp0 = esp0;
    g_tss.ss0 = ss0;
}