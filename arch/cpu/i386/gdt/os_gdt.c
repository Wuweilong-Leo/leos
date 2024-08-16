#include "os_gdt.h"
#include "os_cpu_i386.h"
#include "os_tss.h"
OS_SEC_GDT_DATA struct OsGdtEntry g_gdt[OS_GDT_ENTRY_MAX_NUM] = {
    /* reserved */
    {0},
    /* 代码段 */
    {
        .limitLowWord = 0xFFFF,
        .baseLowWord = 0x0000,
        .baseMidByte = 0x00,
        .attrType = (OS_GDT_ENTRY_ATTR_TYPE_TEXT |
                     OS_GDT_ENTRY_ATTR_TYPE_TEXT_NCOR |
                     OS_GDT_ENTRY_ATTR_TYPE_TEXT_XO |
                     OS_GDT_ENTRY_ATTR_TYPE_NV),
        .attrS = OS_GDT_ENTRY_ATTR_S_NOT_SYS,
        .attrDpl = OS_GDT_ENTRY_ATTR_DPL_0,
        .attrP = OS_GDT_ENTRY_ATTR_P,
        .limitHigh = 0xF,
        .attrAvl = OS_GDT_ENTRY_ATTR_AVL,
        .attrL = OS_GDT_ENTRY_ATTR_L_0,
        .attrDb = OS_GDT_ENTRY_ATTR_DB_1,
        .attrG = OS_GDT_ENTRY_ATTR_G_4K,
        .baseHighByte = 0x00
    },
    /* 数据段 */
    {
        .limitLowWord = 0xFFFF,
        .baseLowWord = 0x0000,
        .baseMidByte = 0x00,
        .attrType = (OS_GDT_ENTRY_ATTR_TYPE_DATA |
                     OS_GDT_ENTRY_ATTR_TYPE_DATA_GROW_UP |
                     OS_GDT_ENTRY_ATTR_TYPE_DATA_RW |
                     OS_GDT_ENTRY_ATTR_TYPE_NV),
        .attrS = OS_GDT_ENTRY_ATTR_S_NOT_SYS,
        .attrDpl = OS_GDT_ENTRY_ATTR_DPL_0,
        .attrP = OS_GDT_ENTRY_ATTR_P,
        .limitHigh = 0xF,
        .attrAvl = OS_GDT_ENTRY_ATTR_AVL,
        .attrL = OS_GDT_ENTRY_ATTR_L_0,
        .attrDb = OS_GDT_ENTRY_ATTR_DB_1,
        .attrG = OS_GDT_ENTRY_ATTR_G_4K,
        .baseHighByte = 0x00
    },
    /* 显存段 */
    {
        .limitLowWord = OS_VIDEO_MEM_PGS_NUM & 0xFFFF,
        .baseLowWord = OS_VIDEO_MEM_BASE & 0xFFFF,
        .baseMidByte = (OS_VIDEO_MEM_BASE & 0x00FF0000) >> 16,
        .attrType = (OS_GDT_ENTRY_ATTR_TYPE_DATA |
                     OS_GDT_ENTRY_ATTR_TYPE_DATA_GROW_UP |
                     OS_GDT_ENTRY_ATTR_TYPE_DATA_RW |
                     OS_GDT_ENTRY_ATTR_TYPE_NV),
        .attrS = OS_GDT_ENTRY_ATTR_S_NOT_SYS,
        .attrDpl = OS_GDT_ENTRY_ATTR_DPL_0,
        .attrP = OS_GDT_ENTRY_ATTR_P,
        .limitHigh = (OS_VIDEO_MEM_PGS_NUM & 0x000F0000) >> 16,
        .attrAvl = OS_GDT_ENTRY_ATTR_AVL,
        .attrL = OS_GDT_ENTRY_ATTR_L_0,
        .attrDb = OS_GDT_ENTRY_ATTR_DB_1,
        .attrG = OS_GDT_ENTRY_ATTR_G_4K,
        .baseHighByte = (OS_VIDEO_MEM_BASE & 0xFF000000) >> 24
    }
};

OS_SEC_GDT_DATA struct OsGdtInfo g_gdtInfo = {
    .gdtLimit = sizeof(g_gdt) - 1,
    .gdtBase = g_gdt,
};

/* 开启分页了，要修改GDT */
OS_SEC_LOADER_TEXT void OsModGdt(void)
{
    /* 修改显存地址 */
    g_gdt[3].baseHighByte = 0xc0;
    
    /* 修改gdt基地址 */
    g_gdtInfo.gdtBase += 0xc0000000;
}

OS_SEC_KERNEL_TEXT void OsBuildGdtEntry(U32 gdtIdx, uintptr_t addr, U32 limit,
                                        U8 attrType, U8 attrS, U8 attrDpl, U8 attrP, U8 attrAvl,
                                        U8 attrL, U8 attrDb, U8 attrG)
{
    struct OsGdtEntry *entry = &g_gdt[gdtIdx];

    entry->limitLowWord = limit & 0xFFFF;
    entry->limitHigh = (limit & 0xF0000) >> 16;
    entry->baseLowWord = (U32)addr & 0xFFFF;
    entry->baseMidByte = ((U32)addr & 0xFF0000) >> 16;
    entry->baseHighByte = ((U32)addr & 0xFF000000) >> 24;

    entry->attrType = attrType;
    entry->attrS = attrS;
    entry->attrDpl = attrDpl;
    entry->attrP = attrP;
    entry->attrAvl = attrAvl;
    entry->attrL = attrL;
    entry->attrDb = attrDb;
    entry->attrG = attrG;
}

OS_INLINE void OsLoadGdt(void)
{
    OS_EMBED_ASM("lgdt %0"::"m"(g_gdtInfo));
}

OS_SEC_KERNEL_TEXT void OsBuildUsrGdtEntry(void)
{
    /* 配置gdt里的tss段 */
    OsTssConfig();

    /* 用户代码段 */
    OsBuildGdtEntry(OS_GDT_USR_CODE_ENTRY_IDX, 0x0U, 0xFFFFFFFF,
                    (OS_GDT_ENTRY_ATTR_TYPE_TEXT | OS_GDT_ENTRY_ATTR_TYPE_TEXT_NCOR |
                    OS_GDT_ENTRY_ATTR_TYPE_TEXT_XO | OS_GDT_ENTRY_ATTR_TYPE_NV),
                    OS_GDT_ENTRY_ATTR_S_NOT_SYS, OS_GDT_ENTRY_ATTR_DPL_3, OS_GDT_ENTRY_ATTR_P, 
                    OS_GDT_ENTRY_ATTR_AVL, OS_GDT_ENTRY_ATTR_L_0, OS_GDT_ENTRY_ATTR_DB_1, OS_GDT_ENTRY_ATTR_G_4K);
    /* 用户数据段 */
    OsBuildGdtEntry(OS_GDT_USR_DATA_ENTRY_IDX, 0x0U, 0xFFFFFFFF,
                    (OS_GDT_ENTRY_ATTR_TYPE_DATA | OS_GDT_ENTRY_ATTR_TYPE_DATA_GROW_UP |
                    OS_GDT_ENTRY_ATTR_TYPE_DATA_RW | OS_GDT_ENTRY_ATTR_TYPE_NV),
                    OS_GDT_ENTRY_ATTR_S_NOT_SYS, OS_GDT_ENTRY_ATTR_DPL_3, OS_GDT_ENTRY_ATTR_P,
                    OS_GDT_ENTRY_ATTR_AVL, OS_GDT_ENTRY_ATTR_L_0, OS_GDT_ENTRY_ATTR_DB_1, OS_GDT_ENTRY_ATTR_G_4K);
    /* 加载gdt */
    OsLoadGdt();
    /* 加载tss */
    OsLoadTss();  
}
