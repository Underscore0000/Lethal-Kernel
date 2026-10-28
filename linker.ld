/*
 * Lethal Kernel - linker script
 * Kernel is loaded at physical/virtual 1 MiB and uses identity mapping.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    _kernel_start = .;

    /* Multiboot2 header must be within the first 32 KiB of the file and
     * 8-byte aligned, so it goes before anything else and is KEEP()ed
     * (otherwise the linker may garbage-collect it since nothing references it).
     */
    .multiboot_header ALIGN(8) : {
        KEEP(*(.multiboot_header))
    }

    .text ALIGN(4096) : {
        *(.text.entry)
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4096) : {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4096) : {
        *(.data)
        *(.data.*)
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