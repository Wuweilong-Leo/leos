CUR_DIR := .
SRC_DIR := $(CUR_DIR)/src
OBJ_DIR := $(CUR_DIR)/build/obj
BIN_DIR := $(CUR_DIR)/build/output
MAP_DIR := $(CUR_DIR)/build/map

# 确保构建目录存在
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR) $(MAP_DIR))

# 编译选项
COMPILE_FLAG := -g -m32 -std=c11 -fno-builtin -fno-stack-protector -fno-pic -fno-pie -Wno-error=int-conversion -Wno-error=incompatible-pointer-types -Wno-error=implicit-function-declaration -DDEBUG_ENABLE=1 -c

# 包含目录
INC_DIR := $(SRC_DIR) \
           $(SRC_DIR)/kernel/include \
           $(SRC_DIR)/arch/cpu/i386 \
           $(SRC_DIR)/arch/hwi/i386 \
           $(SRC_DIR)/arch/exc/i386 \
           $(SRC_DIR)/arch/idt/i386 \
           $(SRC_DIR)/arch/boot \
           $(SRC_DIR)/arch/sys \
           $(SRC_DIR)/arch/timer/i386 \
           $(SRC_DIR)/arch/cpu/i386/gdt \
           $(SRC_DIR)/arch/cpu/i386/pgt \
           $(SRC_DIR)/arch/cpu/i386/tss \
           $(SRC_DIR)/arch/cpu/i386/reset \
           $(SRC_DIR)/arch/io/i386 \
           $(SRC_DIR)/arch/dev/uart \
           $(SRC_DIR)/lib/include \
           $(SRC_DIR)/kernel/sched \
           $(SRC_DIR)/kernel/task/process \
           $(SRC_DIR)/kernel/tick \
           $(SRC_DIR)/kernel/hwi \
           $(SRC_DIR)/kernel/print \
           $(SRC_DIR)/arch/dev/vga \
           $(SRC_DIR)/arch/dev/kbd \
           $(SRC_DIR)/arch/dev/kbd/i386 \
           $(SRC_DIR)/kernel/shell \
           $(SRC_DIR)/kernel/symtab \
           $(SRC_DIR)/debug/include \
           $(SRC_DIR)/kernel/mem \
           $(SRC_DIR)/kernel/mem/fsc \
           $(SRC_DIR)/lib/btmp \
           $(CUR_DIR)/test

INCS := $(addprefix -I, $(INC_DIR))

# 源文件搜索目录
SRC_DIRS := $(SRC_DIR) \
            $(SRC_DIR)/kernel/sched \
            $(SRC_DIR)/kernel/task \
            $(SRC_DIR)/kernel/mem \
            $(SRC_DIR)/kernel/mem/fsc \
            $(SRC_DIR)/kernel/tick \
            $(SRC_DIR)/kernel/hwi \
            $(SRC_DIR)/kernel/ipc/sem \
            $(SRC_DIR)/kernel/ipc/msg \
            $(SRC_DIR)/kernel/task/process \
            $(SRC_DIR)/arch/boot/i386 \
            $(SRC_DIR)/arch/boot \
			$(SRC_DIR)/arch/sys \
            $(SRC_DIR)/arch/io/i386 \
            $(SRC_DIR)/arch/hwi/i386 \
            $(SRC_DIR)/arch/exc/i386 \
            $(SRC_DIR)/arch/idt/i386 \
            $(SRC_DIR)/arch/timer/i386 \
            $(SRC_DIR)/arch/cpu/i386/gdt \
            $(SRC_DIR)/arch/cpu/i386/pgt \
            $(SRC_DIR)/arch/cpu/i386/tss \
            $(SRC_DIR)/lib \
            $(SRC_DIR)/lib/btmp \
            $(SRC_DIR)/arch/cpu/i386 \
            $(SRC_DIR)/arch/cpu/i386/reset \
            $(SRC_DIR)/kernel/print \
            $(SRC_DIR)/arch/dev/vga \
            $(SRC_DIR)/arch/dev/uart/i386 \
            $(SRC_DIR)/arch/dev/kbd/i386 \
            $(SRC_DIR)/kernel/shell \
            $(SRC_DIR)/kernel/symtab \
            $(SRC_DIR)/debug \
            $(CUR_DIR)/test

# 查找所有源文件
C_SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
ASM_SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.S))

# 符号表生成脚本
SYMTAB_SCRIPT := $(CUR_DIR)/tools/gen_symtab.py
SYMTAB_DATA   := $(SRC_DIR)/kernel/symtab/os_symtab_data.c

# 确保 os_symtab_data.c 占位文件存在
ifeq ($(wildcard $(SYMTAB_DATA)),)
$(shell printf '/* placeholder */\n#include "os_def.h"\nstruct OsSymtabEntry { const void *addr; const char *name; };\n#define OS_SYMTAB_ENTRY(sym) { (const void *)&(sym), #sym }\nOS_SEC_KERNEL_DATA const struct OsSymtabEntry g_symtab[] = { {0, 0} };\nOS_SEC_KERNEL_DATA const U32 g_symtabCnt = 0;\n' > $(SYMTAB_DATA))
endif

# 目标文件列表
OBJS := $(patsubst $(CUR_DIR)/%.c, $(OBJ_DIR)/%.o, $(C_SRCS)) \
        $(patsubst $(CUR_DIR)/%.S, $(OBJ_DIR)/%.o, $(ASM_SRCS))

# 主目标
$(BIN_DIR)/os_kernel.elf: $(OBJS)
	@echo "Linking $@"
	ld -T $(SRC_DIR)/ld_script/os_ld.S -m elf_i386 -Map $(MAP_DIR)/kernel.map $^ -o $@
	@echo "Generating symbol table"
	@python3 $(SYMTAB_SCRIPT) $(MAP_DIR)/kernel.map $(SYMTAB_DATA)

# 通用C文件编译规则
$(OBJ_DIR)/%.o: $(CUR_DIR)/%.c
	@mkdir -p $(@D)
	@echo "Compiling $<"
	@gcc $(COMPILE_FLAG) $(INCS) $< -o $@

# 通用汇编文件编译规则
$(OBJ_DIR)/%.o: $(CUR_DIR)/%.S
	@mkdir -p $(@D)
	@echo "Assembling $<"
	@gcc $(COMPILE_FLAG) $(INCS) $< -o $@

# 清理
clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/* $(MAP_DIR)/*

# 反汇编和二进制提取
dis: $(BIN_DIR)/os_kernel.elf
	objcopy -O binary $< $(BIN_DIR)/os_kernel.bin
	objdump -d -S $< > $(BIN_DIR)/os_kernel.dis
	objcopy -O binary -j .MBR $< $(BIN_DIR)/os_mbr.bin
	objcopy -O binary -j .LOADER $< $(BIN_DIR)/os_loader.bin
	objcopy -O binary -j .KERNEL_TEXT -j .KERNEL_BSS -j .KERNEL_DATA $< $(BIN_DIR)/kernel.bin

.PHONY: clean dis
