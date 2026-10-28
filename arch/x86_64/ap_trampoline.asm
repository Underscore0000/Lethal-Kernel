;
; Lethal Kernel - AP Bootstrap Trampoline
; 
; This code is copied to low memory (0x8000) and executed by AP cores
; after receiving a SIPI. It transitions the CPU from real mode to
; protected mode, then to long mode, and finally jumps to the kernel
; AP entry point.
;
; Copyright (c) 2026 Lethal Kernel contributors
; SPDX-License-Identifier: MIT
;

BITS 16
ORG 0x8000

; ============================================================================
; Real-mode entry point
; ============================================================================
ap_trampoline_start:
    cli
    cld

    ; Load real-mode IDT (null)
    lidt [real_idt_ptr]

    ; Enable A20 line (if not already enabled)
    in al, 0x92
    or al, 2
    out 0x92, al

    ; Load temporary GDT for protected mode
    lgdt [gdt32_ptr]

    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to 32-bit code
    jmp 0x08:ap_protected_mode

real_idt_ptr:
    dw 0
    dd 0

; ============================================================================
; Protected mode (32-bit)
; ============================================================================
BITS 32
ap_protected_mode:
    ; Set up segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Set up stack (temporary, will be replaced)
    mov esp, 0x9000
    mov ebp, esp

    ; Enable PAE
    mov eax, cr4
    or eax, (1 << 5)    ; PAE
    or eax, (1 << 7)    ; PGE
    mov cr4, eax

    ; Load PML4 address (from boot CPU)
    mov eax, [pml4_addr]
    mov cr3, eax

    ; Enable long mode
    mov ecx, 0xC0000080  ; EFER MSR
    rdmsr
    or eax, (1 << 8)     ; LME
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, (1 << 31)    ; PG
    mov cr0, eax

    ; Far jump to 64-bit code
    jmp 0x08:ap_long_mode

align 8
gdt32_ptr:
    dw gdt32_end - gdt32 - 1
    dd gdt32

gdt32:
    ; Null descriptor
    dd 0, 0
    ; Code segment (32/64 bit)
    dw 0xFFFF
    dw 0
    db 0
    db 0x9A
    db 0xAF
    db 0
    ; Data segment
    dw 0xFFFF
    dw 0
    db 0
    db 0x92
    db 0xCF
    db 0
gdt32_end:

pml4_addr:
    dd 0
    dd 0

; ============================================================================
; Long mode (64-bit)
; ============================================================================
BITS 64
ap_long_mode:
    ; Set up segment registers for 64-bit mode
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Load kernel GDT (from boot CPU)
    lgdt [ap_gdt_ptr]

    ; Reload segments with kernel selectors
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Jump to kernel AP entry
    mov rax, [ap_entry_pointer]
    jmp rax

; Data for 64-bit mode
ap_gdt_ptr:
    dq 0
    dq 0

ap_entry_pointer:
    dq 0

; Pad to fill a page (so we know the size)
times 4096 - ($ - ap_trampoline_start) db 0
ap_trampoline_end: