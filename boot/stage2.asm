; ============================================================================
; Lethal Kernel - Stage 2 Bootloader
; ============================================================================

bits 16

%include "config.inc"
%include "kernel_size.inc"

org STAGE2_LOAD_OFF

stage2_start:
    mov [boot_drive], dl

    mov si, msg_stage2
    call print_string16

    call serial_init16
    mov si, msg_serial_s2
    call serial_print16

    call get_geometry
    call load_kernel
    call get_memory_map
    mov si, msg_e820
    call print_string16
    call enable_a20
    call verify_a20

    ; --- AGGIUNTA: setup framebuffer ---
    call setup_framebuffer

    mov si, msg_pm
    call print_string16

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE32_SEL:pm_entry

; ============================================================================
; Real mode helpers
; ============================================================================

load_kernel:
    pusha
    mov ax, KERNEL_SECTORS
    test ax, ax
    jz .done
    mov word [remaining], ax
    mov word [cur_lba], DISK_LBA_KERNEL
    mov ax, KERNEL_TEMP_SEG
    mov es, ax
    xor bx, bx
.loop:
    cmp word [remaining], 0
    je .done
    mov ax, [cur_lba]
    call lba_to_chs
    mov ah, 0x02
    mov al, 1
    mov dl, [boot_drive]
    int 0x13
    jc kernel_disk_error
    add bx, 512
    jnz .no_wrap
    mov ax, es
    add ax, 0x1000
    mov es, ax
.no_wrap:
    inc word [cur_lba]
    dec word [remaining]
    jmp .loop
.done:
    mov si, msg_kernel_loaded
    call print_string16
    popa
    ret

kernel_disk_error:
    mov si, msg_kernel_disk_err
    call print_string16
    jmp halt16

FALLBACK_HEADS equ 64
FALLBACK_SPT   equ 32

get_geometry:
    push es
    push di
    xor ax, ax
    mov es, ax
    mov di, ax
    mov dl, [boot_drive]
    mov ah, 0x08
    int 0x13
    jc .use_fallback
    and cl, 0x3F
    movzx ax, cl
    mov [spt_w], ax
    inc dh
    movzx ax, dh
    mov [heads_w], ax
    cmp word [spt_w], 0
    je .use_fallback
    cmp word [heads_w], 0
    je .use_fallback
    pop di
    pop es
    ret
.use_fallback:
    mov word [spt_w], FALLBACK_SPT
    mov word [heads_w], FALLBACK_HEADS
    mov si, msg_geom_fallback
    call print_string16
    pop di
    pop es
    ret

lba_to_chs:
    xor dx, dx
    div word [spt_w]
    mov cl, dl
    inc cl
    xor dx, dx
    div word [heads_w]
    mov ch, al
    mov dh, dl
    mov al, ah
    shl al, 6
    or  cl, al
    ret

enable_a20:
    pusha
    mov ax, 0x2401
    int 0x15
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al
    popa
    ret

verify_a20:
    pusha
    push es
    push ds
    xor ax, ax
    mov ds, ax
    mov si, 0x0500
    mov ax, 0xFFFF
    mov es, ax
    mov di, 0x0510
    mov al, [ds:si]
    mov bl, al
    mov al, [es:di]
    mov bh, al
    mov byte [ds:si], 0x00
    mov byte [es:di], 0xFF
    mov al, [ds:si]
    cmp al, 0xFF
    mov [ds:si], bl
    mov [es:di], bh
    pop ds
    pop es
    popa
    je a20_error
    ret

a20_error:
    mov si, msg_a20_err
    call print_string16
    jmp halt16

E820_BUFFER  equ 0x0000A000
E820_MAX_ENTRIES equ 64

get_memory_map:
    pusha
    push es
    xor ax, ax
    mov es, ax
    mov di, E820_BUFFER
    xor ebx, ebx
    mov word [e820_count], 0
.loop:
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, 24
    int 0x15
    jc .done
    cmp eax, 0x534D4150
    jne .done
    cmp word [e820_count], E820_MAX_ENTRIES
    jae .done
    inc word [e820_count]
    add di, 24
    test ebx, ebx
    jz .done
    jmp .loop
.done:
    pop es
    popa
    ret

e820_count: dw 0

; --- AGGIUNTA: setup_framebuffer ---
setup_framebuffer:
    pusha
    push es
    push ds

    xor ax, ax
    mov es, ax
    mov di, FB_INFO_ADDR
    mov cx, 20
    xor ax, ax
    rep stosw

    mov si, msg_fb_try
    call print_string16

    xor ax, ax
    mov es, ax
    mov di, FB_VBE_MODEINFO
    mov ax, 0x4F01
    mov cx, FB_VBE_MODE
    int 0x10
    cmp ax, 0x004F
    jne .fail

    mov ax, 0x4F02
    mov bx, FB_VBE_MODE | 0x4000
    int 0x10
    cmp ax, 0x004F
    jne .fail

    xor ax, ax
    mov es, ax
    mov di, FB_INFO_ADDR

    mov eax, [FB_VBE_MODEINFO + 0x28]
    mov [es:di + 0], eax
    mov dword [es:di + 4], 0

    movzx eax, word [FB_VBE_MODEINFO + 0x12]
    mov [es:di + 8], eax
    movzx eax, word [FB_VBE_MODEINFO + 0x14]
    mov [es:di + 12], eax
    movzx eax, word [FB_VBE_MODEINFO + 0x10]
    mov [es:di + 16], eax
    movzx eax, byte [FB_VBE_MODEINFO + 0x19]
    mov [es:di + 20], eax

    mov al, [FB_VBE_MODEINFO + 0x1F]
    mov [es:di + 24], al
    mov al, [FB_VBE_MODEINFO + 0x20]
    mov [es:di + 25], al
    mov al, [FB_VBE_MODEINFO + 0x21]
    mov [es:di + 26], al
    mov al, [FB_VBE_MODEINFO + 0x22]
    mov [es:di + 27], al
    mov al, [FB_VBE_MODEINFO + 0x23]
    mov [es:di + 28], al
    mov al, [FB_VBE_MODEINFO + 0x24]
    mov [es:di + 29], al

    mov si, msg_fb_ok
    call print_string16
    jmp .done

.fail:
    mov si, msg_fb_fail
    call print_string16

.done:
    pop ds
    pop es
    popa
    ret

msg_fb_try:  db "Stage2: setting up VBE framebuffer...", 13, 10, 0
msg_fb_ok:   db "Stage2: VBE framebuffer ready (struct at 0x9000)", 13, 10, 0
msg_fb_fail: db "Stage2: VBE setup failed, staying in text mode", 13, 10, 0

; ---------------------------------------------------------------------------
print_string16:
    pusha
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

SERIAL_PORT equ 0x3F8

serial_init16:
    pusha
    mov dx, SERIAL_PORT + 1
    mov al, 0x00
    out dx, al
    mov dx, SERIAL_PORT + 3
    mov al, 0x80
    out dx, al
    mov dx, SERIAL_PORT + 0
    mov al, 0x03
    out dx, al
    mov dx, SERIAL_PORT + 1
    mov al, 0x00
    out dx, al
    mov dx, SERIAL_PORT + 3
    mov al, 0x03
    out dx, al
    mov dx, SERIAL_PORT + 2
    mov al, 0xC7
    out dx, al
    mov dx, SERIAL_PORT + 4
    mov al, 0x0B
    out dx, al
    popa
    ret

serial_print16:
    pusha
.loop:
    lodsb
    test al, al
    jz .done
    call serial_putc16
    jmp .loop
.done:
    popa
    ret

serial_putc16:
    push dx
    push ax
.wait:
    mov dx, SERIAL_PORT + 5
    in al, dx
    test al, 0x20
    jz .wait
    pop ax
    mov dx, SERIAL_PORT
    out dx, al
    pop dx
    ret

halt16:
    cli
.hang:
    hlt
    jmp .hang

boot_drive: db 0
remaining:  dw 0
cur_lba:    dw 0
spt_w:      dw 0
heads_w:    dw 0

msg_stage2:          db "Stage2: real mode init", 13, 10, 0
msg_serial_s2:       db "[stage2] serial online", 13, 10, 0
msg_kernel_loaded:   db "Stage2: kernel image loaded from disk", 13, 10, 0
msg_kernel_disk_err: db "FATAL: disk read error while loading kernel", 13, 10, 0
msg_geometry_error:  db "FATAL: could not query disk geometry", 13, 10, 0
msg_geom_fallback:   db "Stage2: using assumed CHS geometry", 13, 10, 0
msg_a20_err:         db "FATAL: A20 line could not be enabled", 13, 10, 0
msg_pm:              db "Stage2: entering protected mode", 13, 10, 0
msg_e820:            db "Stage2: memory map obtained via INT15h/E820h", 13, 10, 0

align 8
gdt_start:
    dq 0x0000000000000000
gdt_code32:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00
gdt_data32:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00
gdt_code64:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10011010b
    db 00100000b
    db 0x00
gdt_end:
CODE32_SEL equ gdt_code32 - gdt_start
DATA32_SEL equ gdt_data32 - gdt_start
CODE64_SEL equ gdt_code64 - gdt_start
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

bits 32
pm_entry:
    mov ax, DATA32_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, PM_STACK_TOP
    mov dx, SERIAL_PORT + 5
.wait_pm:
    in al, dx
    test al, 0x20
    jz .wait_pm
    mov dx, SERIAL_PORT
    mov al, 'P'
    out dx, al
    call copy_kernel
    call setup_paging
    call enter_long_mode
.hang32:
    cli
    hlt
    jmp .hang32

copy_kernel:
    pushad
    mov esi, KERNEL_TEMP_LINEAR
    mov edi, KERNEL_LINK_ADDR
    mov ecx, KERNEL_BYTES
    add ecx, 3
    shr ecx, 2
    cld
    rep movsd
    popad
    ret

PML4_ADDR equ 0x1000
PDPT_ADDR equ 0x2000
PD_ADDR   equ 0x3000
PAGE_PRESENT equ 1 << 0
PAGE_WRITE   equ 1 << 1
PAGE_SIZE_2M equ 1 << 7

setup_paging:
    pushad
    mov edi, PML4_ADDR
    xor eax, eax
    mov ecx, (3 * 4096) / 4
    cld
    rep stosd
    mov dword [PML4_ADDR], PDPT_ADDR | PAGE_PRESENT | PAGE_WRITE
    mov dword [PDPT_ADDR], PD_ADDR | PAGE_PRESENT | PAGE_WRITE
    mov edi, PD_ADDR
    xor eax, eax
    mov ecx, 8
.fill_pd:
    mov edx, eax
    or edx, PAGE_PRESENT | PAGE_WRITE | PAGE_SIZE_2M
    mov [edi], edx
    mov dword [edi + 4], 0
    add eax, 0x200000
    add edi, 8
    loop .fill_pd
    popad
    ret

enter_long_mode:
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov eax, PML4_ADDR
    mov cr3, eax
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    mov dx, SERIAL_PORT + 5
.wait_lm:
    in al, dx
    test al, 0x20
    jz .wait_lm
    mov dx, SERIAL_PORT
    mov al, 'L'
    out dx, al
    jmp CODE64_SEL:lm_entry

bits 64
lm_entry:
    mov ax, DATA32_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, PM_STACK_TOP
    mov dx, SERIAL_PORT + 5
.wait_km:
    in al, dx
    test al, 0x20
    jz .wait_km
    mov dx, SERIAL_PORT
    mov al, 'K'
    out dx, al

    mov edi, E820_BUFFER
    movzx esi, word [e820_count]
    mov edx, FB_INFO_ADDR

    jmp KERNEL_LINK_ADDR

times (STAGE2_SECTORS * 512) - ($ - $$) db 0