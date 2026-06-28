# leos 测试模块 VGA 行布局与位图详解

## VGA 文本模式 80x25，行号固定

| 行 | 模块 | 格式 | 说明 |
|---|---|---|---|
| 0 | boot | `1 MBR` | MBR 加载成功标志 |
| 2 | task | `A xx` | TaskA (prio=5) delay 10 tick 计数 |
| 3 | task | `B xx` | TaskB (prio=5) delay 20 tick 计数 |
| 4 | task | `C xx` | TaskC (prio=5) delay 30 tick 计数 |
| 5 | task | `D xx` | TaskD (prio=6) 自删除测试，跑 5 圈后自删 |
| 6 | rr | `E xx` | TaskRrE (prio=30) 永不阻塞纯轮转计数 |
| 7 | rr | `F xx` | TaskRrF (prio=30) 永不阻塞纯轮转计数 |
| 8 | sem | `PI: boost=X restore=X ...` | 信号量测试汇总（见下） |
| 9 | exc | `OsExcConfig end` | 异常配置完成 |
| 10 | mem | `MEM: result=0xFF fails=0 ...` | FSC 内存分配器测试 |
| 11 | pgf | `PGF: result=0x7F fails=0 ...` | 缺页测试 |
| 12-18 | init | 各模块初始化日志 | 无需关注 |

## Row 8 信号量测试详解

格式：`PI: boost=X restore=X lowPrio=X->X->X midRan=X highGot=X susOk=X`

| 字段 | 含义 | 正常值 |
|---|---|---|
| boost | PI 提升是否生效 | 1 |
| restore | PI 释放后优先级是否恢复 | 1 |
| lowPrio | PILow 优先级变化 before->during->after | 20->5->20 |
| midRan | PIMid 是否运行过 | 1 |
| highGot | PIHigh 是否获取到 mutex | 1 |
| susOk | Post 是否跳过 SUSPENDED 任务唤醒后续 | 1 |

g_semTestResult 位图（由 TestSemResultCollector 周期性收集）：

```
0x0001  CNT_SEM_OK         计数信号量生产消费正常
0x0002  CNT_POST_FULL      计数信号量 Post 满返回 IS_FULL
0x0004  BIN_SYNC_OK        二值同步信号量正常
0x0008  BIN_POST_REPEAT    二值同步重复 Post 返回 OK
0x0010  MUTEX_OK           二值互斥信号量正常
0x0020  MUTEX_NOT_HOLDER   非持有者 Post 返回 NOT_HOLDER
0x0040  PRIO_WAKE_OK       PRIO 唤醒顺序正确
0x0080  TMO_TIMEOUT        超时等待返回 TIMEOUT
0x0100  TMO_NO_WAIT        不等待返回 UNAVAILABLE
0x0200  TMO_NORMAL         带超时正常获取
0x0400  PI_BOOST_OK        PI: 低优先级持有者被提升
0x0800  PI_RESTORE_OK      PI: 释放后优先级恢复
0x1000  SUSPEND_WAKE_OK    Post 跳过 SUSPENDED 任务唤醒后续
```

全过 = 0x1FFF

## Row 10 MEM 测试位图

```
bit0  basic   基本分配释放
bit1  align   对齐分配
bit2  split   分割回收
bit3  coa     合并回收
bit4  reuse   释放后重用
bit5  oom     内存耗尽处理
bit6  stress  压力测试
bit7  ma      多池分配
```

全过 = 0xFF

## Row 11 PGF 测试位图

```
bit0  fault   基本缺页处理
bit1  inv     无效地址访问
bit2  phys    物理页分配
bit3  guard   guard page 保护
bit4  prim    进程页表操作
bit5  xpde    扩展页目录项
bit6  stress  缺页压力测试
```

全过 = 0x7F

## 时钟与时间片

- PIT 频率: 50Hz (IRQ0_FREQUENCY), COUNTER0_VALUE = 1193180/50
- 1 tick = 20ms
- 默认时间片: 10 tick = 200ms (OS_TASK_TIME_SLICE_DEFAULT)
- OsTaskDelay(ticks) 参数单位为 tick
