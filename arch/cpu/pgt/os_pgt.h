#ifndef OS_PGT_H
#define OS_PGT_H
#include "os_def.h"
struct OS_STRUCT_PACKED OsPgtEntry {
    U8 attrP: 1;
    U8 attrRw: 1;
    U8 attrUs: 1;
    U8 rsvd1: 2;
    U8 attrA: 1;
    U8 attrD: 1;
    U8 rsvd2: 2;
    U8 attrAvl: 3;
    U32 addr: 20;
};

#define OS_PG_P (1 << 0)
#define OS_PG_RW_R (0 << 1)
#define OS_PG_RW_W (1 << 1)
#define OS_PG_US_S (0 << 2)
#define OS_PG_US_U (1 << 2)

#define OS_PGD_ENTRY_NUM 1024
#define OS_PGT_ENTRY_NUM 1024
#endif 