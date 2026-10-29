# ============================================================================
# Lethal Kernel - Build System
#
# Copyright (c) 2026 Lethal Kernel contributors
# SPDX-License-Identifier: MIT
# ============================================================================

NASM    ?= nasm
CC      ?= gcc
LD      ?= ld
OBJCOPY ?= objcopy
GRUB_MKRESCUE ?= grub-mkrescue
QEMU    ?= qemu-system-x86_64
PYTHON  ?= python3
NPROC   ?= $(shell nproc)

BUILD    := build
ISO      := LethalKernel.iso
FS_IMAGE := $(BUILD)/fs.img
FAT_TOOL := tools/mkfat16.py

FONT_FILE      := third_party/fonts/Inter-Regular.ttf

# ----------------------------------------------------------------------------
# QEMU display options
# ----------------------------------------------------------------------------
QEMU_DISPLAY := -display gtk,show-cursor=off,grab-on-hover=on
QEMU_COMMON  := -serial stdio -no-reboot -m 128
QEMU_DISK    := -cdrom $(ISO) -drive file=$(FS_IMAGE),format=raw,if=ide -boot order=d

# ----------------------------------------------------------------------------
# Include paths (LVGL disabled by default)
# ----------------------------------------------------------------------------
INCLUDES := \
	-I kernel \
	-I kernel/include \
	-I kernel/net/dns \
	-I kernel/net/socket
# Uncomment the following lines to re-enable LVGL:
#	-I third_party/lvgl \
#	-I third_party/lvgl/src

# ----------------------------------------------------------------------------
# Kernel CFLAGS
# ----------------------------------------------------------------------------
CFLAGS := -std=gnu11 -Wall -Wextra -Werror \
          -ffreestanding -fno-builtin -fno-stack-protector -fno-stack-check \
          -fno-lto -fno-pic -fno-pie -m64 -mno-red-zone \
          -mno-mmx -mno-sse -mno-sse2 -mno-3dnow \
          -MMD -MP \
          $(INCLUDES) \
          -O2 -c

LDFLAGS := -m elf_x86_64 -T linker.ld -nostdlib -static -no-pie

# ----------------------------------------------------------------------------
# Phony targets
# ----------------------------------------------------------------------------
.PHONY: all kernel clean clean-all run run-fast run-smp run-gdb run-net \
        test test-net test-all fsimage toolcheck help

# ----------------------------------------------------------------------------
# Default target
# ----------------------------------------------------------------------------
all: kernel
	@echo ""
	@echo "==========================================="
	@echo " Kernel compiled. Run 'make run' to build the ISO and filesystem, then start QEMU."
	@echo "==========================================="

kernel: $(BUILD)/kernel.elf
	@echo "Kernel ready."

# ----------------------------------------------------------------------------
# Toolchain check
# ----------------------------------------------------------------------------
toolcheck:
	@command -v $(NASM)    >/dev/null 2>&1 || { echo "ERROR: nasm not found"; exit 1; }
	@command -v $(CC)      >/dev/null 2>&1 || { echo "ERROR: gcc not found"; exit 1; }
	@command -v $(LD)      >/dev/null 2>&1 || { echo "ERROR: ld not found"; exit 1; }
	@command -v $(OBJCOPY) >/dev/null 2>&1 || { echo "ERROR: objcopy not found"; exit 1; }
	@command -v $(GRUB_MKRESCUE) >/dev/null 2>&1 || { echo "ERROR: grub-mkrescue not found (install grub-pc-bin and grub-common)"; exit 1; }
	@command -v $(PYTHON)  >/dev/null 2>&1 || { echo "ERROR: python3 not found"; exit 1; }

# ----------------------------------------------------------------------------
# Kernel sources (excluding VPN, GUI and LVGL by default)
# ----------------------------------------------------------------------------
C_SRCS := $(shell find kernel -name '*.c' -print)

# Exclude VPN
C_SRCS := $(filter-out kernel/net/vpn/%,$(C_SRCS))

# Exclude GUI and all LVGL files
C_SRCS := $(filter-out kernel/gui/%,$(C_SRCS))
C_SRCS := $(filter-out kernel/lib/lvgl_%,$(C_SRCS))
C_SRCS := $(filter-out kernel/core/lvgl_%,$(C_SRCS))

# Uncomment the following line to re-enable LVGL sources:
# C_SRCS += $(shell find third_party/lvgl/src -name '*.c' -print)

ASM_SRCS := $(shell find arch/x86_64 -name '*.asm' -print | grep -v ap_trampoline)
C_OBJS   := $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS))
ASM_OBJS := $(patsubst %.asm,$(BUILD)/%.o,$(ASM_SRCS))
KOBJS    := $(ASM_OBJS) $(C_OBJS) $(BUILD)/arch/x86_64/ap_trampoline.o

-include $(C_OBJS:.o=.d)

# ----------------------------------------------------------------------------
# Compilation rules
# ----------------------------------------------------------------------------
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(NASM) -f elf64 $< -o $@

$(BUILD)/arch/x86_64/ap_trampoline.o: arch/x86_64/ap_trampoline.asm
	@mkdir -p $(dir $@)
	$(NASM) -f bin $< -o $(BUILD)/arch/x86_64/ap_trampoline.bin
	$(OBJCOPY) -I binary -O elf64-x86-64 -B i386:x86-64 \
		--rename-section .data=.rodata \
		$(BUILD)/arch/x86_64/ap_trampoline.bin $@

$(BUILD)/kernel/lib/stb_truetype.o: kernel/lib/stb_truetype.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -msse -msse2 -mfpmath=sse $< -o $@

$(BUILD)/kernel/lib/math.o: kernel/lib/math.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -msse -msse2 -mfpmath=sse $< -o $@

$(BUILD)/kernel/core/console.o: kernel/core/console.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -mno-sse -mno-sse2 -msoft-float $< -o $@

# Uncomment to re-enable lodepng (needed only by LVGL/GUI):
# $(BUILD)/third_party/lvgl/src/libs/lodepng/lodepng.o: third_party/lvgl/src/libs/lodepng/lodepng.c
# 	@mkdir -p $(dir $@)
# 	$(CC) $(CFLAGS) $< -o $@

# ----------------------------------------------------------------------------
# Kernel linking
# ----------------------------------------------------------------------------
$(BUILD)/kernel.elf: $(KOBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KOBJS)
	@echo "Linked kernel.elf"

# ----------------------------------------------------------------------------
# Userspace + FAT16
# ----------------------------------------------------------------------------
USER_BINS := $(BUILD)/user/init.bin $(BUILD)/user/ipc_test.bin

$(BUILD)/user/init.bin: user/init.asm user/lib.asm
	@mkdir -p $(dir $@)
	$(NASM) -f bin -I user/ $< -o $@

$(BUILD)/user/ipc_test.bin: user/ipc_test.asm user/lib.asm
	@mkdir -p $(dir $@)
	$(NASM) -f bin -I user/ $< -o $@

fsimage: $(USER_BINS) $(FAT_TOOL) $(FONT_FILE)
	@mkdir -p $(BUILD)
	@echo "Creating FAT16 image..."
	$(PYTHON) $(FAT_TOOL) $(FS_IMAGE) \
		$(BUILD)/user/init.bin \
		$(BUILD)/user/ipc_test.bin \
		$(FONT_FILE)

# ----------------------------------------------------------------------------
# ISO image
# ----------------------------------------------------------------------------
$(BUILD)/isoroot/boot/kernel.elf: $(BUILD)/kernel.elf
	@mkdir -p $(dir $@)
	cp $< $@

$(BUILD)/isoroot/boot/grub/grub.cfg: boot/grub/grub.cfg
	@mkdir -p $(dir $@)
	cp $< $@

$(ISO): $(BUILD)/isoroot/boot/kernel.elf $(BUILD)/isoroot/boot/grub/grub.cfg
	$(GRUB_MKRESCUE) -o $@ $(BUILD)/isoroot

# ----------------------------------------------------------------------------
# Run targets
# ----------------------------------------------------------------------------
run: $(ISO) fsimage
	$(QEMU) $(QEMU_DISK) $(QEMU_COMMON) $(QEMU_DISPLAY)

run-fast:
	@if [ ! -f $(ISO) ]; then echo "ERROR: $(ISO) does not exist. Run 'make run' first."; exit 1; fi
	@if [ ! -f $(FS_IMAGE) ]; then echo "ERROR: $(FS_IMAGE) does not exist. Run 'make fsimage' first."; exit 1; fi
	$(QEMU) $(QEMU_DISK) $(QEMU_COMMON) $(QEMU_DISPLAY)

run-smp: $(ISO) fsimage
	$(QEMU) $(QEMU_DISK) $(QEMU_COMMON) $(QEMU_DISPLAY) -smp 2

run-gdb: $(ISO) fsimage
	$(QEMU) $(QEMU_DISK) $(QEMU_COMMON) $(QEMU_DISPLAY) -s -S

run-net: $(ISO) fsimage
	$(QEMU) $(QEMU_DISK) $(QEMU_COMMON) $(QEMU_DISPLAY) \
		-netdev user,id=net0 -device e1000,netdev=net0

# ----------------------------------------------------------------------------
# Test targets (headless)
# ----------------------------------------------------------------------------
test: $(ISO) fsimage
	$(QEMU) $(QEMU_DISK) -serial file:$(BUILD)/serial.log -nographic -no-reboot -m 128
	@grep -q "last userspace process exited" $(BUILD)/serial.log && \
		echo "TEST PASSED" || \
		(echo "TEST FAILED" && tail -20 $(BUILD)/serial.log && exit 1)

test-net: $(ISO) fsimage
	$(QEMU) $(QEMU_DISK) -serial file:$(BUILD)/serial-net.log -nographic -no-reboot -m 128 \
		-netdev user,id=net0 -device e1000,netdev=net0
	@grep -q "net" $(BUILD)/serial-net.log && \
		echo "NETWORK TEST PASSED" || \
		(echo "NETWORK TEST FAILED" && tail -20 $(BUILD)/serial-net.log && exit 1)

test-all: test test-net
	@echo "=== ALL TESTS PASSED ==="

# ----------------------------------------------------------------------------
# Cleanup
# ----------------------------------------------------------------------------
clean:
	rm -rf $(BUILD)/*
	@echo "Build directory cleaned."

clean-all:
	rm -rf $(BUILD) $(ISO)
	@echo "All build artifacts removed."

# ----------------------------------------------------------------------------
# Help
# ----------------------------------------------------------------------------
help:
	@echo "Lethal Kernel v7.0 (fast build, GUI disabled by default)"
	@echo ""
	@echo "  make                - Compile the kernel only (fast)"
	@echo "  make run            - Build kernel, ISO, filesystem, and launch QEMU"
	@echo "  make run-fast       - Launch QEMU without rebuilding (very fast)"
	@echo "  make run-smp        - Same as run, but with 2 CPUs"
	@echo "  make run-net        - Same as run, but with an e1000 network interface"
	@echo "  make run-gdb        - Same as run, but with the GDB stub enabled (-s -S)"
	@echo "  make fsimage        - Build only the FAT16 filesystem image"
	@echo "  make clean          - Clean the build/ directory"
	@echo "  make clean-all      - Remove all build artifacts"
	@echo "  make help           - Display this help message"
	@echo ""
	@echo "  GUI and LVGL are disabled by default."
	@echo "  To re-enable them, uncomment the lines marked in the Makefile."
	@echo ""
	@echo "  QEMU display options: -display gtk,show-cursor=off,grab-on-hover=on"
	@echo ""
	@echo "  Tip: use 'make -j$(NPROC)' for the initial compilation."