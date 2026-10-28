# Lethal Kernel Alpha Version

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Architecture: x86_64](https://img.shields.io/badge/Architecture-x86__64-blue.svg)]()
[![Boot: Multiboot2](https://img.shields.io/badge/Boot-Multiboot2-green.svg)]()
[![Language: C + Assembly](https://img.shields.io/badge/Language-C%20%2B%20Assembly-orange.svg)]()
[![Status: Experimental](https://img.shields.io/badge/Status-Experimental-red.svg)]()

A from-scratch x86_64 operating system kernel with a full TCP/IP stack, FAT16 filesystem, SMP scheduler, PS/2 drivers, and a TTF framebuffer console. No external dependencies, no GUI. Just the foundations.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Repository Structure](#repository-structure)
- [Requirements](#requirements)
- [Building](#building)
- [Usage](#usage)
- [Project Status](#project-status)
- [Known Limitations](#known-limitations)
- [Enabling the GUI (LVGL)](#enabling-the-gui-lvgl)
- [Contributing](#contributing)
- [License](#license)
- [Credits](#credits)
- [Contact](#contact)

---

## Overview

**Lethal Kernel** is a monolithic x86_64 kernel booted via GRUB (Multiboot2). It implements the entire stack from the bootloader handoff to the network stack, without relying on any external OS or library.

LVGL is included in the repository but **disabled by default** and can be re-enabled for future GUI work.

The project is intended for educational and research purposes. Every component, including paging, heap, scheduler, drivers, filesystem, and networking, is implemented from scratch in C and assembly.

---

## Features

### Boot and Architecture

- Multiboot2 boot protocol (GRUB)
- Long mode (x86_64) with GDT, IDT, TSS
- 4-level paging (PML4, PDPT, PD, PT), identity-mapped kernel
- PMM (Physical Memory Manager) with Multiboot2 memory map parsing
- VMM (Virtual Memory Manager) with page mapping and unmapping
- Heap with first-fit allocator, block coalescing, and self-test
- SMP with LAPIC, I/O APIC, AP trampoline, and per-CPU data
- Multi-CPU scheduler with cooperative and preemptive tasks
- PIT at 100 Hz, RTC for wall-clock time

### Drivers

- PS/2 keyboard (IRQ1) with full keymap: letters, digits, symbols, Shift, Caps Lock, Alt, Ctrl, F1-F12, arrows, Home/End, PgUp/PgDn, Insert, Delete
- PS/2 mouse (IRQ12) with wheel support (Intellimouse 4-byte packets) and polling fallback
- ATA PIO storage driver
- VBE linear framebuffer with configurable color depth
- Serial port for debug output

### Filesystem

- FAT16 with automatic mount, file read, and partition detection
- VFS (Virtual Filesystem Switch) with `open`, `read`, `write`, `close`, `size`
- Automatic boot filesystem detection (FAT16, EXT2/3/4)

### Networking

- Full stack: Ethernet, ARP, IPv4, ICMP, TCP, UDP, DNS, DoH (DNS over HTTPS)
- e1000 (Intel PRO/1000) driver with MMIO support
- POSIX-like socket layer (`socket`, `bind`, `listen`, `accept`, `connect`, `send`, `recv`, `close`)
- Privacy features: MAC randomization, TCP timestamp randomization, ICMP replies disabled, source port randomization, ARP spoof protection
- Killswitch and optional Tor integration
- Self-tests for every networking layer

### Console and Terminal

- VGA text mode fallback (80x25, before framebuffer init)
- TTF framebuffer console with Inter-Regular font, antialiasing, scrolling, and UTF-8 support
- Diagnostic shell (F2) with commands: `help`, `exit`, `clear`, `echo`
- Keyboard test mode (K) that prints raw scancodes and decoded keys

### Self-Tests

Every subsystem has an integrated self-test:

- PMM alloc, write, free
- VMM map, write, unmap
- Heap (100-node list, calloc, realloc, free)
- Spinlock and mutex
- Scheduler (cooperative tasks with interleaved yield)
- SMP (CPU count)
- Filesystem (mount)
- Networking (IP, `sk_buff`, socket, ARP, ICMP, TCP, UDP, DNS, DoH, privacy, killswitch, Tor, devices)

---

## Repository Structure
Lethal-Kernel/
├── arch/x86_64/ # Entry point, GDT, IDT, IRQ, SMP, AP trampoline
├── boot/grub/ # GRUB configuration
├── kernel/
│ ├── core/ # kernel_main, console, panic, spinlock, mutex, terminal
│ ├── drivers/ # PS/2 keyboard/mouse, ATA, framebuffer, serial
│ ├── fs/ # VFS, FAT16, EXT2/3/4, filesystem detection
│ ├── memory/ # PMM, VMM, heap
│ ├── net/ # Full networking stack
│ ├── process/ # Tasks, scheduler
│ ├── smp/ # Multi-CPU support
│ ├── ipc/ # Pipes
│ └── lib/ # stb_truetype, math, string utilities
├── third_party/
│ ├── lvgl/ # LVGL v9 (disabled by default, kept for future GUI)
│ ├── fonts/ # Inter-Regular.ttf
│ └── wallpapers/ # Wallpaper PNGs
├── tools/
│ └── mkfat16.py # FAT16 image builder
├── user/ # Userspace programs (skeleton)
├── Makefile
├── linker.ld
├── LICENSE # MIT
└── README.md

text

### Networking Stack (`kernel/net/`)
kernel/net/
├── core/ # net.c, device.c, sk_buff.c, ethernet.c
├── ipv4/ # ip.c, arp.c, icmp.c, tcp.c, udp.c, ip_route.c
├── dns/ # dns.c, dns_cache.c, doh.c
├── socket/ # socket.c
├── config/ # net_config.c, privacy.c
├── killswitch/ # killswitch.c
├── tor/ # tor.c, tor_control.c
└── vpn/ # VPN (excluded from build by default)

text

---

## Requirements

### Build Dependencies

| Tool | Purpose | Debian/Ubuntu Package |
|------|---------|-----------------------|
| `gcc` | C compiler | `gcc` |
| `ld` | GNU linker | `binutils` |
| `nasm` | Assembler | `nasm` |
| `grub-mkrescue` | ISO creation | `grub-pc-bin` `grub-common` |
| `python3` | FAT16 image builder | `python3` |
| `qemu-system-x86_64` | Testing | `qemu-system-x86` |
| `make` | Build system | `make` |

### Runtime

- x86_64 CPU with long mode support
- At least 128 MiB of RAM
- VBE linear framebuffer (provided by GRUB)
- ATA device (for filesystem)
- Intel e1000 NIC (optional, for networking)

---

## Building

### Clone and Build

```bash
git clone https://github.com/Underscore0000/Lethal-Kernel.git
cd Lethal-Kernel
make -j$(nproc)
make run
Available Make Targets
Target	Description
make	Build the kernel only (fast)
make run	Build kernel + ISO + FAT16 and launch QEMU
make run-fast	Launch QEMU without rebuilding
make run-smp	Launch QEMU with 2 CPUs
make run-net	Launch QEMU with e1000 network interface
make run-gdb	Launch QEMU with GDB stub (-s -S)
make fsimage	Build only the FAT16 filesystem image
make icons	Check that PNG icons exist in third_party/icons_ready/
make clean	Clean the build/ directory
make clean-all	Remove all build artifacts (build + ISO)
Usage
Boot
Launch QEMU with make run.

The kernel boots and prints all self-tests to the TTF framebuffer console.

At the end of the boot, the diagnostic shell appears.

Diagnostic Shell
Key	Action
K	Keyboard test (prints scancodes and decoded keys)
F2	Open the terminal
ESC	Power off the system
Terminal Commands
Command	Description
help	Show command list
exit	Close the terminal
clear	Clear the screen
echo <text>	Print the given text
Project Status
Component	Status
Multiboot2 boot	Stable
PMM / VMM / Heap	Stable
SMP / Scheduler	Stable
PS/2 keyboard	Stable
PS/2 mouse (with wheel)	Stable
ATA PIO	Stable
FAT16 / VFS	Stable
Networking (IP, TCP, UDP, DNS, DoH)	Stable
TTF framebuffer console	Stable
Diagnostic shell	Stable
Terminal	Basic (help, exit, clear, echo)
Ping command	In progress
Userspace applications	Skeleton
GUI (LVGL)	Disabled (kept in repo for future work)
Known Limitations
No USB support.

The filesystem is read-only from the kernel (writing limited to FAT16 image creation).

The terminal does not yet support the ping command (in progress).

The GUI (LVGL) is included in the repository but disabled by default.

The TCP stack is minimal (no congestion control, no SACK).

No audio support.

No userspace runtime (programs in user/ are skeleton).

Enabling the GUI (LVGL)
The GUI was removed from the default build, but the LVGL sources and the GUI code are kept in the repository for future integration.

To re-enable:

1. Uncomment the LVGL include paths in the Makefile:

make
INCLUDES := \
    -I kernel \
    -I kernel/include \
    -I third_party/lvgl \
    -I third_party/lvgl/src
2. Uncomment the LVGL sources in C_SRCS:

make
C_SRCS += $(shell find third_party/lvgl/src -name '*.c' -print)
3. Restore the kernel/gui/ directory and the corresponding calls in kernel/core/kernel.c.

Contributing
Contributions are welcome, especially in these areas:

Bug fixes for any subsystem.

Networking: TCP congestion control, SACK, IPv6.

Filesystem: write support for FAT16, EXT2/3/4.

Drivers: USB, audio, more NICs.

GUI: re-enable LVGL and build a desktop environment on top.

Userspace: implement a real shell, file manager, text editor.

Before submitting a pull request:

Build with make clean && make -j$(nproc).

Run the kernel in QEMU with make run.

Ensure all self-tests pass.

Follow the existing code style (kernel-style C, no external dependencies).

License
Distributed under the MIT License. See LICENSE for details.

text
MIT License

Copyright (c) 2026 Lethal Kernel contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
Credits
LVGL — lvgl.io (MIT License, disabled by default)

stb_truetype — Sean Barrett (public domain / MIT)

lodepng — Lode Vandevenne (zlib License)

GRUB — GNU Project (GPLv3)

Contact
GitHub: @Underscore0000

Repository: Lethal-Kernel
