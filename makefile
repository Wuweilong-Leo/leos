CUR_DIR := ./
OBJ_DIR := $(CUR_DIR)/build/obj
BIN_DIR := $(CUR_DIR)/build/output
MAP_DIR := $(CUR_DIR)/build/map

COMPILE_FLAG := -m32 -std=c99 -fno-builtin -c

INC_DIR := $(CUR_DIR)  
INC_DIR += $(CUR_DIR)/kernel/include/
INC_DIR += $(CUR_DIR)/arch/cpu/
INC_DIR += $(CUR_DIR)/arch/cpu/gdt
INC_DIR += $(CUR_DIR)/arch/cpu/pgt
INC_DIR += $(CUR_DIR)/arch/io/
INC_DIR += $(CUR_DIR)/lib/include/
INC_DIR += $(CUR_DIR)/kernel/sched

INCS = $(foreach dir, $(INC_DIR), -I$(dir))		   

SUB_DIR := $(CUR_DIR)
SUB_DIR += $(CUR_DIR)/kernel/sched
SUB_DIR += $(CUR_DIR)/kernel/task
SUB_DIR += $(CUR_DIR)/kernel/mem
SUB_DIR += $(CUR_DIR)/arch/boot/i386
SUB_DIR += $(CUR_DIR)/arch/io
SUB_DIR += $(CUR_DIR)/arch/cpu/gdt
SUB_DIR += $(CUR_DIR)/arch/cpu/pgt
SUB_DIR += $(CUR_DIR)/lib
SUB_DIR += $(CUR_DIR)/arch/cpu/

C_SRCS := $(foreach dir, $(SUB_DIR), $(wildcard $(dir)/*.c))
C_SRCS_NO_DIR := $(notdir $(C_SRCS))
C_OBJS := $(patsubst %.c, $(OBJ_DIR)/%.o, $(C_SRCS_NO_DIR))

ASM_SRCS := $(foreach dir, $(SUB_DIR), $(wildcard $(dir)/*.S))
ASM_SRCS_NO_DIR = $(notdir $(ASM_SRCS))
ASM_OBJS := $(patsubst %.S, $(OBJ_DIR)/%.o, $(ASM_SRCS_NO_DIR))

OBJS := $(ASM_OBJS) $(C_OBJS)

$(BIN_DIR)/os_kernel.elf:$(OBJS)
	ld -T ld_script/os_ld.S -m elf_i386 -Map $(MAP_DIR)/kernel.map $^ -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/kernel/sched/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/kernel/task/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/kernel/mem/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/io/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/cpu/gdt/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/cpu/pgt/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/lib/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/boot/i386/%.S 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/cpu/%.S 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

clean:
	rm -fr $(CUR_DIR)/build/map/*
	rm -fr $(CUR_DIR)/build/obj/*
	rm -fr $(CUR_DIR)/build/output/*

dis:
	objcopy -O binary $(CUR_DIR)/build/output/os_kernel.elf $(CUR_DIR)/build/output/os_kernel.bin
	objdump -d -S $(CUR_DIR)/build/output/os_kernel.elf > $(CUR_DIR)/build/output/os_kernel.dis
	objcopy -O binary -j .L1M_MBR $(CUR_DIR)/build/output/os_kernel.elf $(CUR_DIR)/build/output/os_mbr.bin
	objcopy -O binary -j .L1M_LOADER $(CUR_DIR)/build/output/os_kernel.elf $(CUR_DIR)/build/output/os_loader.bin
	objcopy -O binary -j .ENTRY -j .L1M_TEXT -j .L1M_BSS -j .L1M_DATA -j .L2M_TEXT -j .L2M_BSS -j .L2M_DATA $(CUR_DIR)/build/output/os_kernel.elf $(CUR_DIR)/build/output/kernel.bin