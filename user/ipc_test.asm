bits 64
org 0

_start:
    sub rsp, 16
    mov rdi, rsp
    mov rsi, rsp
    add rsi, 8
    call pipe
    test rax, rax
    jnz .fail

    mov r12, [rsp]
    mov r13, [rsp + 8]
    add rsp, 16
    
    call fork
    test rax, rax
    jz .child

.parent:
    mov rdi, r13
    lea rsi, [rel msg]
    mov rdx, msg_len
    call write
    mov rdi, r13
    call close
    mov rdi, 1
    lea rsi, [rel parent_done]
    mov rdx, parent_done_len
    call write
    xor rdi, rdi
    call exit

.child:
    mov rdi, r12
    lea rsi, [rel buffer]
    mov rdx, 64
    call read
    mov r14, rax
    mov rdi, r12
    call close
    mov rdi, 1
    lea rsi, [rel child_msg]
    mov rdx, child_msg_len
    call write
    mov rdi, 1
    lea rsi, [rel buffer]
    mov rdx, r14
    call write
    xor rdi, rdi
    call exit

.fail:
    mov rdi, 1
    lea rsi, [rel fail_msg]
    mov rdx, fail_msg_len
    call write
    mov rdi, 1
    call exit

msg db '[pipe] Hello from parent!',10
msg_len equ $ - msg

parent_done db '[user] v0.12 IPC test complete',10
parent_done_len equ $ - parent_done

child_msg db '[user] child received via pipe: '
child_msg_len equ $ - child_msg

buffer times 64 db 0

fail_msg db '[user] ERROR: pipe test failed',10
fail_msg_len equ $ - fail_msg

%include "lib.asm"