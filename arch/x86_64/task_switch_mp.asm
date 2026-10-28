bits 64
section .text

; void mp_task_switch(struct mp_task *old, struct mp_task *new)
; rdi = old, rsi = new
global mp_task_switch
mp_task_switch:
    ; Save old task context
    mov [rdi + 16], rsp   ; rsp offset
    mov [rdi + 24], rbx   ; rbx offset
    mov [rdi + 32], rbp   ; rbp offset
    mov [rdi + 40], r12   ; r12 offset
    mov [rdi + 48], r13   ; r13 offset
    mov [rdi + 56], r14   ; r14 offset
    mov [rdi + 64], r15   ; r15 offset
    
    ; Load new task context
    mov rsp, [rsi + 16]
    mov rbx, [rsi + 24]
    mov rbp, [rsi + 32]
    mov r12, [rsi + 40]
    mov r13, [rsi + 48]
    mov r14, [rsi + 56]
    mov r15, [rsi + 64]
    
    ret

; mp_task_bootstrap - entry point for new tasks
global mp_task_bootstrap
extern mp_task_bootstrap_c
mp_task_bootstrap:
    pop rdi                 ; entry function pointer
    call mp_task_bootstrap_c
    ; Should never return
.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits