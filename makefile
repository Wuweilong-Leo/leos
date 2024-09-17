CUR_DIR := ./
OBJ_DIR := $(CUR_DIR)/build/obj
BIN_DIR := $(CUR_DIR)/build/output
MAP_DIR := $(CUR_DIR)/build/map

COMPILE_FLAG := -g -m32 -std=c11 -fno-builtin -c

INC_DIR := $(CUR_DIR)  
INC_DIR += $(CUR_DIR)/kernel/include/
INC_DIR += $(CUR_DIR)/arch/cpu/i386/
INC_DIR += $(CUR_DIR)/arch/hwi/i386
INC_DIR += $(CUR_DIR)/arch/boot/
INC_DIR += $(CUR_DIR)/arch/timer/i386
INC_DIR += $(CUR_DIR)/arch/cpu/i386/gdt
INC_DIR += $(CUR_DIR)/arch/cpu/i386/pgt
INC_DIR += $(CUR_DIR)/arch/cpu/i386/tss
INC_DIR += $(CUR_DIR)/arch/cpu/i386/
INC_DIR += $(CUR_DIR)/arch/io/i386
INC_DIR += $(CUR_DIR)/lib/include/
INC_DIR += $(CUR_DIR)/kernel/sched
INC_DIR += $(CUR_DIR)/kernel/task/process
INC_DIR += $(CUR_DIR)/kernel/tick
INC_DIR += $(CUR_DIR)/dev/print
INC_DIR += $(CUR_DIR)/dev/include
INC_DIR += $(CUR_DIR)/debug/include
INC_DIR += $(CUR_DIR)/kernel/mem
INC_DIR += $(CUR_DIR)/lib/btmp

INCS = $(foreach dir, $(INC_DIR), -I$(dir))		   

SUB_DIR := $(CUR_DIR)
SUB_DIR += $(CUR_DIR)/kernel/sched
SUB_DIR += $(CUR_DIR)/kernel/task
SUB_DIR += $(CUR_DIR)/kernel/mem
SUB_DIR += $(CUR_DIR)/kernel/tick
SUB_DIR += $(CUR_DIR)/kernel/ipc/sem
SUB_DIR += $(CUR_DIR)/kernel/task/process
SUB_DIR += $(CUR_DIR)/arch/boot/i386
SUB_DIR += $(CUR_DIR)/arch/boot
SUB_DIR += $(CUR_DIR)/arch/io/i386
SUB_DIR += $(CUR_DIR)/arch/hwi/i386
SUB_DIR += $(CUR_DIR)/arch/timer/i386
SUB_DIR += $(CUR_DIR)/arch/cpu/i386/gdt
SUB_DIR += $(CUR_DIR)/arch/cpu/i386/pgt
SUB_DIR += $(CUR_DIR)/arch/cpu/i386/tss
SUB_DIR += $(CUR_DIR)/lib
SUB_DIR += $(CUR_DIR)/lib/btmp
SUB_DIR += $(CUR_DIR)/arch/cpu/i386
SUB_DIR += $(CUR_DIR)/dev/print
SUB_DIR += $(CUR_DIR)/debug
SUB_DIR += $(CUR_DIR)/arch/cpu/i386/

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

$(OBJ_DIR)/%.o: $(CUR_DIR)/kernel/task/process/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/kernel/mem/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/io/i386/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/cpu/i386/gdt/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/cpu/i386/pgt/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/cpu/i386/tss/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/lib/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/lib/btmp/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/debug/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/dev/print/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/hwi/i386/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/timer/i386/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/boot/i386/%.S 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/boot/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/cpu/i386/%.S 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/arch/cpu/i386/%.c 
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/kernel/ipc/sem/%.c
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

$(OBJ_DIR)/%.o: $(CUR_DIR)/kernel/tick/%.c
	gcc $(COMPILE_FLAG) $(INCS) $< -o $@

clean:
	rm -fr $(CUR_DIR)/build/map/*
	rm -fr $(CUR_DIR)/build/obj/*
	rm -fr $(CUR_DIR)/build/output/*

dis:
	objcopy -O binary $(CUR_DIR)/build/output/os_kernel.elf $(CUR_DIR)/build/output/os_kernel.bin
	objdump -d -S $(CUR_DIR)/build/output/os_kernel.elf > $(CUR_DIR)/build/output/os_kernel.dis
	objcopy -O binary -j .MBR $(CUR_DIR)/build/output/os_kernel.elf $(CUR_DIR)/build/output/os_mbr.bin
	objcopy -O binary -j .LOADER $(CUR_DIR)/build/output/os_kernel.elf $(CUR_DIR)/build/output/os_loader.bin
	objcopy -O binary -j .KERNEL_TEXT -j .KERNEL_BSS -j .KERNEL_DATA $(CUR_DIR)/build/output/os_kernel.elf $(CUR_DIR)/build/output/kernel.bin