; ============================================================================
; Lethal Kernel - Stage 1 Bootloader (Master Boot Record)
; ============================================================================

bits 16

%include "config.inc"

org STAGE1_LOAD_OFF

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STAGE1_LOAD_OFF
    sti

    mov [boot_drive], dl
    mov si, msg_banner
    call print_string
    call get_geometry

    mov word [cur_lba], DISK_LBA_STAGE2
    mov word [remaining], STAGE2_SECTORS
    mov ax, STAGE2_LOAD_SEG
    mov es, ax
    mov bx, STAGE2_LOAD_OFF

.load_loop:
    cmp word [remaining], 0
    je load_done
    mov ax, [cur_lba]
    call lba_to_chs
    mov ah, 0x02
    mov al, 1
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    add bx, 512
    inc word [cur_lba]
    dec word [remaining]
    jmp .load_loop

load_done:
    mov si, msg_ok
    call print_string
    mov dl, [boot_drive]
    jmp STAGE2_LOAD_SEG:STAGE2_LOAD_OFF

disk_error:
    mov si, msg_disk_error
    call print_string
    jmp halt

halt:
    cli
.hang:
    hlt
    jmp .hang

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
    mov byte [sectors_per_track], cl
    movzx ax, cl
    mov [spt_w], ax
    inc dh
    mov byte [num_heads], dh
    movzx ax, dh
    mov [heads_w], ax
    cmp byte [sectors_per_track], 0
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
    call print_string
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

print_string:
    pusha
    mov ah, 0x0E
    mov bh, 0
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

boot_drive:          db 0
sectors_per_track:   db 0
num_heads:           db 0
spt_w:                dw 0
heads_w:              dw 0
cur_lba:              dw 0
remaining:            dw 0

msg_banner:         db "Lethal Kernel: Stage1", 13, 10, 0
msg_ok:             db "Stage1: Stage2 loaded, handing off...", 13, 10, 0
msg_disk_error:     db "FATAL: disk read error while loading Stage2", 13, 10, 0
msg_geom_fallback:  db "Stage1: using assumed CHS geometry", 13, 10, 0

times 510-($-$$) db 0
dw 0xAA55