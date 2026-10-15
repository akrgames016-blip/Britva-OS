# Britva OS

Britva OS is a **bare-metal 32-bit x86 operating-system prototype** written in freestanding C++ and GNU Assembly. The build produces a legacy-BIOS floppy image whose boot sector loads the kernel, enters protected mode, and hands off to the kernel's exception/IRQ, timer, keyboard, memory, VGA, and serial code.

This is real OS source and a buildable disk image—not a web mockup or a desktop OS. QEMU is not installed in this workspace, so the image's actual boot has not been verified here. It is an educational prototype, not a general-purpose system.

## Implemented in v0.2.0

- BIOS boot sector reads the kernel from a 1.44 MB floppy using CHS, queries conventional/extended memory, enables A20, loads a flat GDT, and enters 32-bit protected mode.
- Freestanding i386 C++ kernel with its own entry point, stack, and `.bss` initialization.
- IDT exception handlers for vectors 0–31; fatal exceptions print their vector and halt.
- 8259 PIC remap, 100 Hz PIT timer IRQ, uptime counter, and interrupt-driven PS/2 keyboard input.
- Interrupt-driven COM1 receive (IRQ4) and serial output for headless VMs.
- VGA 80×25 text console with hardware cursor, scrolling, and selectable text colors.
- Simple bitmap-based 4 KiB physical page-frame allocator, initialized from BIOS-reported RAM; `palloc` and `pfree` exercise it.
- Small embedded **read-only RAM filesystem** with `README.TXT`, `MOTD.TXT`, and `LIMITS.TXT`.
- Interactive kernel shell: `help`, `about`, `version`, `uname`, `whoami`, `mem`, `uptime`, `palloc`, `pfree`, `echo`, `ls`, `cat`, `color`, `clear`, `reboot`, and `halt`.
- Python reference shell simulator with the same command vocabulary where host-side behavior can be represented safely. It is not presented as a Python kernel.

## Build and run

### Requirements

GNU Make, GNU C++ (`g++`), GNU binutils (`as`, `ld`, `objcopy`), Python 3, and QEMU (`qemu-system-i386`) to run the OS. The 32-bit kernel is freestanding; it does not need 32-bit host libraries.

On Debian/Ubuntu:

```sh
sudo apt install build-essential binutils make python3 qemu-system-x86
```

Build the 1.44 MB BIOS floppy image:

```sh
make
```

Output: `build/britva-os.img`. A compressed v0.2.0 image and checksum are in [`release-assets/`](release-assets/README.md).

Start QEMU with the VGA console and mirrored serial terminal:

```sh
make run
```

Or use the serial console in the current terminal:

```sh
make run-serial
```

The serial terminal accepts shell commands. Press **Ctrl+A**, then **X** to quit QEMU.

Boot and exercise the COM1 shell automatically:

```sh
make qemu-test
```

## Python reference simulator

```sh
python3 python/britva.py
python3 python/britva.py --command "echo Hello from Britva"
python3 python/britva.py --demo
```

This only runs a shell reference simulator in the host Python process. It does not boot or emulate the C++ kernel, read host memory, execute host commands, or reboot the host.

## Tests

```sh
make test
```

The tests cover the Python command model and floppy-image builder. The QEMU smoke test requires `qemu-system-i386`; run `make qemu-test` to verify an actual VM boot and shell response.

## Current limits

Britva OS is intentionally an early, text-mode kernel. It currently has **no** paging/virtual memory, ring-3/user mode, scheduler or multitasking, persistent disk filesystem, process loader, networking, graphics stack, USB, or UEFI boot. The page allocator is deliberately basic: it uses a bitmap over the contiguous range reported by legacy BIOS `INT 15h/AH=88h`, not an E820 map, and the kernel has not enabled paging. The embedded RAM filesystem is compiled into the kernel and is read-only.

No synthetic screenshots or browser UI are used as proof of operation. A real screenshot should be captured from the OS in QEMU (or on hardware); this checkout has no QEMU executable, so the VM boot smoke test and real screen capture cannot be run here. Use `make qemu-test` and `make run` on a machine with QEMU installed.

## License

MIT. See [LICENSE](LICENSE).
