bits 64

global malloc
global brk
global fork
global write
global read
global exit
global yield
global pipe
global close
global sleep
global sys_wait

malloc:
    mov rax, 3
    int 0x80
    ret

brk:
    mov rax, 4
    int 0x80
    ret

fork:
    mov rax, 5
    int 0x80
    ret

write:
    mov rax, 1
    int 0x80
    ret

read:
    mov rax, 8
    int 0x80
    ret

exit:
    mov rax, 2
    int 0x80
    ret

yield:
    mov rax, 6
    int 0x80
    ret

pipe:
    mov rax, 7
    int 0x80
    ret

close:
    mov rax, 9
    int 0x80
    ret

sleep:
    mov rax, 10
    int 0x80
    ret

sys_wait:
    cmp rdi, 0x0000000040000000
    jb .wait_pid_mode
    mov rsi, rdi
    mov rdi, -1
    jmp .wait_do
.wait_pid_mode:
.wait_do:
    mov rax, 11
    int 0x80
    ret