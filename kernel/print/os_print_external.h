#ifndef OS_PRINT_EXTERNAL_H
#define OS_PRINT_EXTERNAL_H
#include "os_def.h"

/* ====== 可变参数宏 ====== */

#define OS_VA_START(ap, v) ((ap) = (void *)&(v))
#define OS_VA_ARG(ap, t)   (*(t *)((ap) += 4))
#define OS_VA_END(ap)       ((ap) = NULL)

/* ====== 打印钩子注册 ====== */

struct OsPrintOps {
    void (*setCursor)(U16 pos);       /* 设置光标位置 */
    U16 (*getCursor)(void);           /* 获取光标位置 */
    void (*writeChar)(U32 pos, char c, U8 attr); /* 写字符到显示设备 */
    void (*scrollUp)(void);           /* 向上滚一行 */
    void (*clearLine)(U32 row);       /* 清空指定行 */
    void (*mirrorChar)(char c);       /* 镜像字符到辅助输出(如串口)，NULL=不镜像 */
    U16 colNum;   /* 列数 */
    U16 posNum;   /* 总字符位置数 */
    U8 attrDefault; /* 默认属性 */
};

/* 由架构层调用，注册输出设备钩子 */
extern void OsPrintRegisterOps(const struct OsPrintOps *ops);

/* ====== 打印 API ====== */

extern void OsPrintChar(char c);
extern void OsPrintStr(char *str);
extern void OsPrintHex(U32 num);
extern void OsPrintSetCursor(U16 target);
extern U16 OsPrintGetCursor(void);
extern S32 kprintf(const char *fmt, ...);

#endif /* OS_PRINT_EXTERNAL_H */
