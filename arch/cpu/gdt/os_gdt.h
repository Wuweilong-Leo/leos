#ifndef OS_GDT_H
#define OS_GDT_H
#include "os_def.h"
struct os_gdt_entry_attr_low {
  uint8_t type : 4;
  uint8_t s : 1;
  uint8_t dpl : 2;
  uint8_t p : 1;
};

struct os_gdt_entry_attr_high {
  uint8_t avl : 1;
  uint8_t l : 1;
  uint8_t db : 1;
  uint8_t g : 1;
};

struct os_gdt_entry {
  uint16_t limit_low_word;
  uint16_t base_low_word;
  uint8_t base_mid_byte;
  struct os_gdt_entry_attr_low attr_low;
  uint8_t limit_high : 4;
  struct os_gdt_entry_attr_high attr_high;
  uint8_t base_high_byte;
};

#define OS_GDT_ENTRY_MAX_NUM 0x10
#endif