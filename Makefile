CXX ?= g++
AS ?= as
LD ?= ld
OBJCOPY ?= objcopy
PYTHON ?= python3
QEMU ?= qemu-system-i386

BUILD := build
KERNEL_CXXFLAGS := -m32 -march=i386 -std=c++17 -O2 \
	-ffreestanding -fno-builtin -fno-exceptions -fno-rtti \
	-fno-stack-protector -fno-pic -fno-pie -fno-threadsafe-statics \
	-fno-use-cxa-atexit -fno-asynchronous-unwind-tables -fno-unwind-tables \
	-mno-sse -mno-mmx -Wall -Wextra -Wpedantic

.PHONY: all os python test qemu-test run run-serial clean preview help

all: $(BUILD)/britva-os.img
os: all

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: boot/boot.S | $(BUILD)
	$(AS) --32 $< -o $@

$(BUILD)/boot.bin: $(BUILD)/boot.o boot/boot.ld
	$(LD) -m elf_i386 -T boot/boot.ld --oformat binary -o $@ $<
	test "$$(wc -c < $@)" -eq 512

$(BUILD)/entry.o: kernel/entry.S | $(BUILD)
	$(AS) --32 $< -o $@

$(BUILD)/kernel.o: kernel/kernel.cpp | $(BUILD)
	$(CXX) $(KERNEL_CXXFLAGS) -c $< -o $@

$(BUILD)/kernel.elf: $(BUILD)/entry.o $(BUILD)/kernel.o kernel/linker.ld
	$(LD) -m elf_i386 -nostdlib --build-id=none -T kernel/linker.ld -o $@ $(BUILD)/entry.o $(BUILD)/kernel.o

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BUILD)/britva-os.img: $(BUILD)/boot.bin $(BUILD)/kernel.bin tools/mkimage.py
	$(PYTHON) tools/mkimage.py $(BUILD)/boot.bin $(BUILD)/kernel.bin $@

python:
	$(PYTHON) python/britva.py

test:
	$(PYTHON) -m unittest discover -s tests -v

qemu-test: $(BUILD)/britva-os.img
	$(PYTHON) tools/qemu_smoke.py --require

run: $(BUILD)/britva-os.img
	$(QEMU) -m 64M -boot order=a -drive format=raw,file=$(BUILD)/britva-os.img,if=floppy -serial stdio

run-serial: $(BUILD)/britva-os.img
	$(QEMU) -m 64M -boot order=a -display none -monitor none -serial stdio -drive format=raw,file=$(BUILD)/britva-os.img,if=floppy

preview:
	$(PYTHON) -m http.server 4173 --bind 0.0.0.0 --directory preview

clean:
	rm -rf $(BUILD)

help:
	@echo "make all          Build the bootable BIOS floppy image"
	@echo "make test         Run Python edition and image-builder tests"
	@echo "make qemu-test    Smoke-test the serial shell under QEMU"
	@echo "make run          Boot in QEMU (VGA window + serial mirror)"
	@echo "make run-serial   Boot QEMU in the current terminal"
	@echo "make python       Run the Python host simulation"
	@echo "make preview      Serve the browser demo on port 4173"
