bits 64
section .text

global user_process_return_trampoline
extern user_process_finished
extern kernel_stack_top

; SYS_EXIT changes the IRET frame to ring0 and lands here. A same-CPL IRET
; leaves the old user RSP/SS words on the interrupt stack, so we deliberately
; discard that abandoned frame by switching to the known kernel stack.
user_process_return_trampoline:
    mov rsp, kernel_stack_top
    xor rbp, rbp
    call user_process_finished

.hang:
    cli
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
