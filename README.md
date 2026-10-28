Lethal Kernel alpha version
--------------------------------------------------------------------
A from-scratch x86_64 operating system kernel with a full TCP/IP stack, FAT16 filesystem, SMP scheduler, PS/2 drivers, and a TTF framebuffer console. No external dependencies, no GUI. Just the foundations.

Overview
Lethal Kernel is a monolithic x86_64 kernel booted via GRUB (Multiboot2). It implements the entire stack from the bootloader handoff to the network stack, without relying on any external OS or library. LVGL is included in the repository but disabled by default and can be re-enabled for future GUI work. The project is intended for educational and research purposes. Every component, including paging, heap, scheduler, drivers, filesystem, and networking, is implemented from scratch in C and assembly.

Features
Boot and architecture
Multiboot2 boot protocol (GRUB)

Long mode (x86_64) with GDT, IDT, TSS

4-level paging (PML4, PDPT, PD, PT), identity-mapped kernel

PMM (Physical Memory Manager) with Multiboot2 memory map parsing

VMM (Virtual Memory Manager) with page mapping and unmapping

Heap with first-fit allocator, block coalescing, and self-test

SMP with LAPIC, I/O APIC, AP trampoline, and per-CPU data

Multi-CPU scheduler with cooperative and preemptive tasks

PIT at 100 Hz, RTC for wall-clock time

Drivers
PS/2 keyboard (IRQ1) with full keymap: letters, digits, symbols, Shift, Caps Lock, Alt, Ctrl, F1-F12, arrows, Home/End, PgUp/PgDn, Insert, Delete

PS/2 mouse (IRQ12) with wheel support (Intellimouse 4-byte packets) and polling fallback

ATA PIO storage driver

VBE linear framebuffer with configurable color depth

Serial port for debug output

Filesystem
FAT16 with automatic mount, file read, and partition detection

VFS (Virtual Filesystem Switch) with open, read, write, close, size

Automatic boot filesystem detection (FAT16, EXT2/3/4)

Networking
Full stack: Ethernet, ARP, IPv4, ICMP, TCP, UDP, DNS, DoH (DNS over HTTPS)

e1000 (Intel PRO/1000) driver with MMIO support

POSIX-like socket layer (socket, bind, listen, accept, connect, send, recv, close)

Privacy features: MAC randomization, TCP timestamp randomization, ICMP replies disabled, source port randomization, ARP spoof protection

Killswitch and optional Tor integration

Self-tests for every networking layer

Console and terminal
VGA text mode fallback (80x25, before framebuffer init)

TTF framebuffer console with Inter-Regular font, antialiasing, scrolling, and UTF-8 support

Diagnostic shell (F2) with commands: help, exit, clear, echo

Keyboard test mode (K) that prints raw scancodes and decoded keys

Self-tests
Every subsystem has an integrated self-test: PMM alloc, write, free; VMM map, write, unmap; Heap (100-node list, calloc, realloc, free); Spinlock and mutex; Scheduler (cooperative tasks with interleaved yield); SMP (CPU count); Filesystem (mount); Networking (IP, sk_buff, socket, ARP, ICMP, TCP, UDP, DNS, DoH, privacy, killswitch, Tor, devices).

Repository structure
text
Lethal-Kernel/
├── arch/x86_64/
├── boot/grub/
├── kernel/
│   ├── core/
│   ├── drivers/
│   ├── fs/
│   ├── memory/
│   ├── net/
│   ├── process/
│   ├── smp/
│   ├── ipc/
│   └── lib/
├── third_party/
│   ├── lvgl/
│   ├── fonts/
│   └── wallpapers/
├── tools/
├── user/
├── Makefile
├── linker.ld
├── LICENSE
└── README.md
Requirements
Build
gcc, ld, nasm, grub-mkrescue, python3, qemu-system-x86_64, make.

Runtime
x86_64 CPU with long mode support, at least 128 MiB of RAM, VBE linear framebuffer (provided by GRUB), ATA device (for filesystem), Intel e1000 NIC (optional, for networking).

Building
bash
git clone https://github.com/Underscore0000/Lethal-Kernel.git
cd Lethal-Kernel
make -j$(nproc)
make run
Available targets
make: build the kernel only. make run: build kernel + ISO + FAT16 and launch QEMU. make run-fast: launch QEMU without rebuilding. make run-smp: launch QEMU with 2 CPUs. make run-net: launch QEMU with e1000 network interface. make run-gdb: launch QEMU with GDB stub (-s -S). make fsimage: build only the FAT16 filesystem image. make icons: check that PNG icons exist in third_party/icons_ready/. make clean: clean the build/ directory. make clean-all: remove all build artifacts.

Usage
Launch QEMU with make run. The kernel boots and prints all self-tests to the TTF framebuffer console. At the end of the boot, the diagnostic shell appears.

Diagnostic shell
K: keyboard test. F2: open the terminal. ESC: power off the system.

Terminal
Commands available: help, exit, clear, echo.

Project status
Multiboot2 boot: Stable. PMM / VMM / Heap: Stable. SMP / Scheduler: Stable. PS/2 keyboard: Stable. PS/2 mouse (with wheel): Stable. ATA PIO: Stable. FAT16 / VFS: Stable. Networking (IP, TCP, UDP, DNS, DoH): Stable. TTF framebuffer console: Stable. Diagnostic shell: Stable. Terminal: Basic. Ping command: In progress. Userspace applications: Skeleton. GUI (LVGL): Disabled, kept in repo for future work.

Known limitations
No USB support. The filesystem is read-only from the kernel. The terminal does not yet support the ping command. The GUI (LVGL) is included in the repository but disabled by default. The TCP stack is minimal (no congestion control, no SACK). No audio support. No userspace runtime.

Enabling the GUI (LVGL)
The GUI was removed from the default build, but the LVGL sources and the GUI code are kept in the repository for future integration. To re-enable: uncomment the LVGL include paths in the Makefile, uncomment the LVGL sources in C_SRCS, restore the kernel/gui/ directory and the corresponding calls in kernel/core/kernel.c.

Contributing
Contributions are welcome, especially in these areas: bug fixes for any subsystem; networking (TCP congestion control, SACK, IPv6); filesystem write support; drivers (USB, audio, more NICs); GUI re-enablement; userspace applications. Before submitting a pull request: build with make clean && make -j$(nproc); run the kernel in QEMU with make run; ensure all self-tests pass; follow the existing code style.

License
Distributed under the MIT License. See LICENSE for details.

Credits
LVGL (MIT, disabled by default), stb_truetype (public domain / MIT), lodepng (zlib), GRUB (GPLv3).

Contact
GitHub: https://github.com/Underscore0000
Repository: https://github.com/Underscore0000/Lethal-Kernel
