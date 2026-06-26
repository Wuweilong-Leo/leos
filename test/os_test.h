#ifndef OS_TEST_H
#define OS_TEST_H

#include "os_def.h"
#include "os_test_framework.h"

/* 测试初始化入口（由 OsConfigInit 表调用） */
extern U32 OsAppConfigInit(void);

/* 总测试任务入口 */
extern void OsTestMainTask(void *arg1, void *arg2, void *arg3, void *arg4);

#endif /* OS_TEST_H */
