; ============================================================================
; Lethal Kernel - Multiboot2 entry point
; ============================================================================

section .multiboot_header progbits alloc noexec nowrite align=8

multiboot_header_start:
    dd 0xE85250D6
    dd 0
    dd multiboot_header_end - multiboot_header_start
    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header_start))

    align 8
    dw 5
    dw 0
    dd 20
    dd 1024
    dd 768
    dd 32

    align 8
    dw 0
    dw 0
    dd 8
multiboot_header_end:

; ---------------------------------------------------------------------------
; 32-bit bootstrap
; ---------------------------------------------------------------------------
section .text.entry
bits 32

global _start
extern kernel_main
extern __bss_start
extern __bss_end
global kernel_stack_top

_start:
    ; Salva magic e info_addr in memoria (variabili .data)
    mov [saved_magic], eax
    mov [saved_info], ebx

    ; Carica la nostra GDT (con selettore 64-bit a 0x08)
    lgdt [gdt_descriptor]

    ; Stack temporaneo
    mov esp, stack32_top

    ; Costruisci page table
    mov edi, 0x1000
    xor eax, eax
    mov ecx, (3 * 4096) / 4
    cld
    rep stosd

    mov dword [0x1000], 0x2000 | 0x3
    mov dword [0x2000], 0x3000 | 0x3

    mov edi, 0x3000
    xor eax, eax
    mov ecx, 8
.fill_pd:
    mov edx, eax
    or edx, 0x83
    mov [edi], edx
    mov dword [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    loop .fill_pd

    ; PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; CR3
    mov eax, 0x1000
    mov cr3, eax

    ; EFER.LME
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; CR0.PG
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Salta in long mode
    jmp 0x08:long_mode_entry

; ---------------------------------------------------------------------------
; 64-bit entry
; ---------------------------------------------------------------------------
bits 64
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov rsp, kernel_stack_top

    ; --- Abilita SSE (necessario per stb_truetype e math.c) ---
    mov rax, cr0
    and rax, ~(1 << 2)      ; CR0.EM = 0 (disabilita emulazione FPU)
    or  rax, (1 << 1)       ; CR0.MP = 1 (monitor coprocessore)
    mov cr0, rax

    mov rax, cr4
    or  rax, (1 << 9)       ; CR4.OSFXSR = 1 (abilita SSE)
    or  rax, (1 << 10)      ; CR4.OSXMMEXCPT = 1 (abilita eccezioni SSE)
    mov cr4, rax

    ; Zero .bss
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor al, al
    cld
    rep stosb

    ; Ricarica magic e info_addr dalla memoria
    mov edi, [saved_magic]
    mov esi, [saved_info]
    call kernel_main

.hang64:
    cli
    hlt
    jmp .hang64

; ---------------------------------------------------------------------------
; GDT
; ---------------------------------------------------------------------------
align 8
gdt_start:
    dq 0x0000000000000000
gdt_code64:
    dq 0x00209A0000000000
gdt_data64:
    dq 0x0000920000000000
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ---------------------------------------------------------------------------
; Variabili salvate
; ---------------------------------------------------------------------------
section .data
align 8
saved_magic:
    dd 0
saved_info:
    dd 0

; ---------------------------------------------------------------------------
; Stack
; ---------------------------------------------------------------------------
section .bss
align 16
stack32_bottom:
    resb 4096
stack32_top:

align 16
kernel_stack_bottom:
    resb 16384
kernel_stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits