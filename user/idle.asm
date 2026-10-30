bits 64
org 0

; IDLE.BIN - PID 0 user task
; Runs forever, yielding the CPU so other READY tasks can run.

_start:
.loop:
    call yield
    jmp .loop

%include "lib.asm"