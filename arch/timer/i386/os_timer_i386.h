#ifndef OS_TIMER_H
#define OS_TIMER_H

#define IRQ0_FREQUENCY 100
#define INPUT_FREQUENCY 1193180
#define COUNTER0_VALUE (INPUT_FREQUENCY / IRQ0_FREQUENCY) // 初始计数值
#define COUNTER0_PORT 0x40
#define COUNTER0_NO 0
#define COUNTER_MODE 2
#define READ_WRITE_LATCH 3
#define PIT_CONTROL_PORT 0x43
#define OS_TIMER_BUILD_FREQ_PORT_MODE(counterNum, rwl, counterMode) ((U8)((counterNum << 6) | (rwl << 4) | (counterMode << 1)))

extern void OsTimerConfig(void);
#endif