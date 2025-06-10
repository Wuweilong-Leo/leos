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
#endif