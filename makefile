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
           $(CUR_DIR)/config/i386 \
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
           $(SRC_DIR)/kernel/sys \
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
            $(SRC_DIR)/kernel/sys \
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

# 符号表生成脚本 + 占位符
# 必须在 C_SRCS 收集之前确保 os_symtab_data.c 存在，否则 wildcard 收不到它，
# OBJS 缺 os_symtab_data.o，ON 链接报 g_symtab 未定义。
# 占位符同样用 #ifdef OS_OPTION_SYMTAB 包裹，与 gen_symtab.py 生成的真表语义一致
# （OFF 时不产出 g_symtab，避免引入无人引用的死数据）。
SYMTAB_SCRIPT := $(CUR_DIR)/tools/gen_symtab.py
SYMTAB_DATA   := $(SRC_DIR)/kernel/symtab/os_symtab_data.c
ifeq ($(wildcard $(SYMTAB_DATA)),)
$(shell printf '/* placeholder */\n#include "os_def.h"\n#include "os_target.h"\nstruct OsSymtabEntry { const void *addr; const char *name; };\n#define OS_SYMTAB_ENTRY(sym) { (const void *)&(sym), #sym }\n#ifdef OS_OPTION_SYMTAB\nOS_SEC_KERNEL_DATA const struct OsSymtabEntry g_symtab[] = { {0, 0} };\nOS_SEC_KERNEL_DATA const U32 g_symtabCnt = 0;\n#endif\n' > $(SYMTAB_DATA))
endif

# 查找所有源文件
C_SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
ASM_SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.S))

# 目标文件列表
OBJS := $(patsubst $(CUR_DIR)/%.c, $(OBJ_DIR)/%.o, $(C_SRCS)) \
        $(patsubst $(CUR_DIR)/%.S, $(OBJ_DIR)/%.o, $(ASM_SRCS))

# 主目标：两阶段链接
# stage1 用占位符 os_symtab_data.o 链接，提取 kernel.map（Os* 符号在 .KERNEL_TEXT，
#   不受 .KERNEL_DATA 段占位符大小影响，地址已定）；
# 生成真版 os_symtab_data.c 后，stage2 重编 data.o 并重新链接，得到含真符号表的 elf。
# 一次 make 即出真表，无需手动二次 make；删除 data.c 也能自愈（占位符自动重建）。
SYMTAB_OBJ := $(OBJ_DIR)/src/kernel/symtab/os_symtab_data.o
$(BIN_DIR)/os_kernel.elf: $(OBJS) $(SYMTAB_SCRIPT)
	@echo "Linking $@ (stage 1: extract kernel.map)"
	ld -T $(SRC_DIR)/ld_script/os_ld.S -m elf_i386 -Map $(MAP_DIR)/kernel.map $(OBJS) -o $@.stage1
	@echo "Generating symbol table"
	@python3 $(SYMTAB_SCRIPT) $(MAP_DIR)/kernel.map $(SYMTAB_DATA)
	@echo "Recompiling symtab data (stage 2)"
	@gcc $(COMPILE_FLAG) $(INCS) $(SYMTAB_DATA) -o $(SYMTAB_OBJ)
	@echo "Linking $@ (stage 2: final)"
	ld -T $(SRC_DIR)/ld_script/os_ld.S -m elf_i386 -Map $(MAP_DIR)/kernel.map $(OBJS) -o $@
	@rm -f $@.stage1

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
