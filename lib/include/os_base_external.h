#ifndef OS_BASE_EXTERNAL_H
#define OS_BASE_EXTERNAL_H
#include "os_def.h"

OS_INLINE U32 OsGetLmb(U32 val)
{
    U32 tmp = val;
    U32 idx = 0;

    while (tmp != 0) {
        if ((tmp & (0x80000000U >> idx)) != 0) {
            return idx;
        }

        tmp &= ~(0x80000000U >> idx);
        idx++;
    }

    return idx;
}

/* 获取结构体内元素的偏移 */
#define OS_OFFSET(structType, elem) ((uintptr_t)(&(((structType *)0)->elem)))

/* 通过元素地址获取结构体的首地址 */
#define OS_GET_STRUCT_ENTRY(structType, elemName, elemAddr)               \
  ((structType *)((uintptr_t)(elemAddr) - OS_OFFSET(structType, elemName)))
#endif