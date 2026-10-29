/*
 * Lethal Kernel - linker script
 *
 * Copyright (c) 2026 Lethal Kernel contributors
 * SPDX-License-Identifier: MIT
 */
ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    _kernel_start = .;

    .multiboot_header ALIGN(8) : {
        KEEP(*(.multiboot_header))
    }

    .text ALIGN(4096) : {
        _text_start = .;
        *(.text.entry)
        *(.text)
        *(.text.*)
        _text_end = .;
    }

    .rodata ALIGN(4096) : {
        _rodata_start = .;
        *(.rodata)
        *(.rodata.*)
        _rodata_end = .;
    }

    .data ALIGN(4096) : {
        _data_start = .;
        *(.data)
        *(.data.*)
        _data_end = .;
    }

    .bss ALIGN(4096) : {
        __bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        __bss_end = .;
    }

    _kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note.*)
    }
}