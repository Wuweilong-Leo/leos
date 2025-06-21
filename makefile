CUR_DIR := .
OBJ_DIR := $(CUR_DIR)/build/obj
BIN_DIR := $(CUR_DIR)/build/output
MAP_DIR := $(CUR_DIR)/build/map

# 确保构建目录存在
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR) $(MAP_DIR))

# 编译选项
COMPILE_FLAG := -g -m32 -std=c11 -fno-builtin -c

# 包含目录
INC_DIR := $(CUR_DIR) \
           $(CUR_DIR)/kernel/include \
           $(CUR_DIR)/arch/cpu/i386 \
           $(CUR_DIR)/arch/hwi/i386 \
           $(CUR_DIR)/arch/boot \
           $(CUR_DIR)/arch/sys \
           $(CUR_DIR)/arch/timer/i386 \
           $(CUR_DIR)/arch/cpu/i386/gdt \
           $(CUR_DIR)/arch/cpu/i386/pgt \
           $(CUR_DIR)/arch/cpu/i386/tss \
           $(CUR_DIR)/arch/io/i386 \
           $(CUR_DIR)/lib/include \
           $(CUR_DIR)/kernel/sched \
           $(CUR_DIR)/kernel/task/process \
           $(CUR_DIR)/kernel/tick \
           $(CUR_DIR)/dev/print \
           $(CUR_DIR)/dev/include \
           $(CUR_DIR)/debug/include \
           $(CUR_DIR)/kernel/mem \
           $(CUR_DIR)/kernel/mem/fsc \
           $(CUR_DIR)/lib/btmp

INCS := $(addprefix -I, $(INC_DIR))

# 源文件搜索目录
SRC_DIRS := $(CUR_DIR) \
            $(CUR_DIR)/kernel/sched \
            $(CUR_DIR)/kernel/task \
            $(CUR_DIR)/kernel/mem \
            $(CUR_DIR)/kernel/mem/fsc \
            $(CUR_DIR)/kernel/tick \
            $(CUR_DIR)/kernel/ipc/sem \
            $(CUR_DIR)/kernel/task/process \
            $(CUR_DIR)/arch/boot/i386 \
            $(CUR_DIR)/arch/boot \
			$(CUR_DIR)/arch/sys \
            $(CUR_DIR)/arch/io/i386 \
            $(CUR_DIR)/arch/hwi/i386 \
            $(CUR_DIR)/arch/timer/i386 \
            $(CUR_DIR)/arch/cpu/i386/gdt \
            $(CUR_DIR)/arch/cpu/i386/pgt \
            $(CUR_DIR)/arch/cpu/i386/tss \
            $(CUR_DIR)/lib \
            $(CUR_DIR)/lib/btmp \
            $(CUR_DIR)/arch/cpu/i386 \
            $(CUR_DIR)/dev/print \
            $(CUR_DIR)/debug

# 查找所有源文件
C_SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
ASM_SRCS := $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.S))

# 目标文件列表
OBJS := $(patsubst $(CUR_DIR)/%.c, $(OBJ_DIR)/%.o, $(C_SRCS)) \
        $(patsubst $(CUR_DIR)/%.S, $(OBJ_DIR)/%.o, $(ASM_SRCS))

# 主目标
$(BIN_DIR)/os_kernel.elf: $(OBJS)
	@echo "Linking $@"
	ld -T ld_script/os_ld.S -m elf_i386 -Map $(MAP_DIR)/kernel.map $^ -o $@

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