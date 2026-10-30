bits 64
org 0

; SLTEST.BIN - fork + exit(7) test (no sleep, no wait)

_start:
    mov rdi, 1
    lea rsi, [rel msg_parent_start]
    mov rdx, msg_parent_start_end - msg_parent_start
    call write

    call fork
    cmp rax, 0
    je .child

    mov rdi, 1
    lea rsi, [rel msg_parent_forked]
    mov rdx, msg_parent_forked_end - msg_parent_forked
    call write

    xor rdi, rdi
    call exit

.child:
    mov rdi, 1
    lea rsi, [rel msg_child_start]
    mov rdx, msg_child_start_end - msg_child_start
    call write

    mov rdi, 7
    call exit

msg_parent_start:
    db '[SLTEST] parent start', 13, 10
msg_parent_start_end:

msg_parent_forked:
    db '[SLTEST] fork -> child', 13, 10
msg_parent_forked_end:

msg_child_start:
    db '[SLTEST] child start', 13, 10
msg_child_start_end:

%include "lib.asm"