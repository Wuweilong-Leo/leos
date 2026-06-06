#ifndef OS_PRINT_INTERNAL_H
#define OS_PRINT_INTERNAL_H

#include "os_def.h"

#define OS_VA_START(ap, v) ((ap) = (void *)&(v))
#define OS_VA_ARG(ap, t)   (*(t *)((ap) += 4))
#define OS_VA_END(ap)       ((ap) = NULL)

/* 输出设备操作钩子，由架构层注册 */
struct OsPrintOps {
    void (*setCursor)(U16 pos);
    U16 (*getCursor)(void);
    void (*writeChar)(U32 pos, char c, U8 attr);
    void (*scrollUp)(void);
    void (*clearLine)(U32 row);
    U16 colNum;   /* 列数 */
    U16 posNum;   /* 总字符位置数 */
    U8 attrDefault; /* 默认属性 */
};

extern struct OsPrintOps g_printOps;

/* 由架构层调用，注册输出设备钩子 */
extern void OsPrintRegisterOps(const struct OsPrintOps *ops);

#endif /* OS_PRINT_INTERNAL_H */