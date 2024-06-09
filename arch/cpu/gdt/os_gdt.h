#ifndef OS_GDT_H
#define OS_GDT_H
#include "os_def.h"

struct OS_STRUCT_PACKED OsGdtEntry {
  U16 limitLowWord;
  U16 baseLowWord;
  U8 baseMidByte;
  U8 attrType : 4;
  U8 attrS : 1;
  U8 attrDpl : 2;
  U8 attrP : 1;
  U8 limitHigh : 4;
  U8 attrAvl : 1;
  U8 attrL : 1;
  U8 attrDb : 1;
  U8 attrG : 1;
  U8 baseHighByte;
};

struct OS_STRUCT_PACKED OsGdtInfo {
  U16 gdtLimit;
  U32 gdtBase;
};

#define OS_GDT_ENTRY_MAX_NUM 0x10
#define OS_GDT_ENTRY_ATTR_DPL_0 0
#define OS_GDT_ENTRY_ATTR_DPL_1 1
#define OS_GDT_ENTRY_ATTR_DPL_2 2
#define OS_GDT_ENTRY_ATTR_DPL_3 3
#define OS_GDT_ENTRY_ATTR_S_0 0
#define OS_GDT_ENTRY_ATTR_S_1 1
#define OS_GDT_ENTRY_ATTR_NP 0
#define OS_GDT_ENTRY_ATTR_P 1
#define OS_GDT_ENTRY_ATTR_L_0 0
#define OS_GDT_ENTRY_ATTR_L_1 1
#define OS_GDT_ENTRY_ATTR_DB_0 0
#define OS_GDT_ENTRY_ATTR_DB_1 1
#define OS_GDT_ENTRY_ATTR_G_4K 1
#define OS_GDT_ENTRY_ATTR_G_B 0
#define OS_GDT_ENTRY_ATTR_TYPE_DATA (0 << 3)
#define OS_GDT_ENTRY_ATTR_TYPE_TEXT (1 << 3)
#define OS_GDT_ENTRY_ATTR_TYPE_DATA_GROW_UP (0 << 2)
#define OS_GDT_ENTRY_ATTR_TYPE_DATA_GROW_DOWN (1 << 2)
#define OS_GDT_ENTRY_ATTR_TYPE_DATA_RO (0 << 1)
#define OS_GDT_ENTRY_ATTR_TYPE_DATA_RW (1 << 1)
#define OS_GDT_ENTRY_ATTR_TYPE_TEXT_NCOR (0 << 2)
#define OS_GDT_ENTRY_ATTR_TYPE_TEXT_COR (1 << 2)
#define OS_GDT_ENTRY_ATTR_TYPE_TEXT_XO (0 << 1)
#define OS_GDT_ENTRY_ATTR_TYPE_TEXT_XR (1 << 1)
#define OS_GDT_ENTRY_ATTR_TYPE_V (0 << 0)
#define OS_GDT_ENTRY_ATTR_TYPE_NV (0 << 0)
#define OS_GDT_ENTRY_ATTR_AVL 0

extern struct OsGdtEntry g_gdt[OS_GDT_ENTRY_MAX_NUM];
extern uintptr_t _os_gdt_start;
#endif