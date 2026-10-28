bits 64
org 0

_start:
    lea rsi, [rel msg_start]
    mov rdi, 1
    mov rdx, msg_start_end - msg_start
    call write

    mov rdi, 4096
    call malloc
    test rax, rax
    jz .fail
    mov r12, rax
    mov byte [r12], 'P'

    call fork
    cmp rax, 0
    je .child

.parent:

    mov byte [r12], 'P'
    mov rdi, 1
    lea rsi, [rel parent_msg]
    mov rdx, parent_msg_end-parent_msg
    call write
    mov ecx, 5
.parent_yield:
    call yield
    loop .parent_yield
    mov rdi, 1
    lea rsi, [rel parent_done]
    mov rdx, parent_done_end-parent_done
    call write
    xor rdi, rdi
    call exit

.child:

    mov byte [r12], 'C'
    mov rdi, 1
    lea rsi, [rel child_msg]
    mov rdx, child_msg_end-child_msg
    call write
    xor rdi, rdi
    call exit

.fail:
    mov rdi, 1
    lea rsi, [rel fail_msg]
    mov rdx, fail_msg_end-fail_msg
    call write
    mov rdi, 1
    call exit

msg_start db '[user] malloc(4096) via SYS_MMAP + fork() COW test',13,10
msg_start_end:
child_msg db '[user] child: COW write completed',13,10
child_msg_end:
parent_msg db '[user] parent: private COW page active',13,10
parent_msg_end:
parent_done db '[user] v0.11 memory test complete',13,10
parent_done_end:
fail_msg db '[user] ERROR: malloc failed',13,10
fail_msg_end:


%include "lib.asm"