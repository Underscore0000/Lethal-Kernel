bits 64

; Minimal v0.12 userspace ABI with IPC support
global malloc
global brk
global fork
global write
global read
global exit
global yield
global pipe
global close

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