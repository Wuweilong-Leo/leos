#ifndef OS_TEST_H
#define OS_TEST_H

#include "os_def.h"

/* 测试初始化入口（由 OsConfigInit 表调用） */
extern U32 OsAppConfigInit(void);

/* 各测试模块 */
extern U32 OsTestTaskInit(void);
extern U32 OsTestSemInit(void);
extern U32 OsTestMemInit(void);
extern U32 OsTestPgFaultInit(void);
extern U32 OsTestRrInit(void);

#endif /* OS_TEST_H */