bits 64

section .text

global user_enter_ring3

; Enter ring3 using an explicitly constructed IRETQ frame.
; SysV64 arguments:
;   RDI = user RIP
;   RSI = user RSP
;   RDX = user CS
;   RCX = user SS
;
; This routine never returns.
user_enter_ring3:
    cli

    ; Load the user data selector before iretq.  These segment registers are
    ; not part of the iret frame and must be valid when execution continues
    ; in CPL3.
    mov ax, cx
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; iretq consumes, from the current RSP:
    ;   RIP, CS, RFLAGS, RSP, SS
    push rcx
    push rsi
    pushfq
    or qword [rsp], 0x200
    push rdx
    push rdi
    iretq

.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
