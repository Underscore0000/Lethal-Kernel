bits 64
section .text

; void task_switch(struct task_context *old_ctx, struct task_context *new_ctx)
; rdi = old_ctx, rsi = new_ctx
;
; Salva i registri callee-saved + rsp corrente in *old_ctx, poi carica gli
; stessi campi da *new_ctx e ritorna: se new_ctx e' un task gia' avviato in
; precedenza, "ret" riprende esattamente da dove quel task aveva chiamato
; task_switch() l'ultima volta. Se new_ctx e' un task nuovo, lo stack e'
; stato preparato da task_create() in modo che "ret" salti a
; task_bootstrap_trampoline.
global task_switch
task_switch:
    mov [rdi + 0],  rbx
    mov [rdi + 8],  rbp
    mov [rdi + 16], r12
    mov [rdi + 24], r13
    mov [rdi + 32], r14
    mov [rdi + 40], r15
    mov [rdi + 48], rsp      ; old_ctx->rsp (rip non serve: e' in cima allo stack)

    mov rbx, [rsi + 0]
    mov rbp, [rsi + 8]
    mov r12, [rsi + 16]
    mov r13, [rsi + 24]
    mov r14, [rsi + 32]
    mov r15, [rsi + 40]
    mov rsp, [rsi + 48]      ; new_ctx->rsp

    ret                       ; salta a [rsp], impostato da task_create()
                              ; o dal precedente task_switch() di quel task

; Punto di ingresso per un task mai eseguito prima. task_create() ha
; preparato lo stack cosi': [rsp] = task_bootstrap_trampoline (e' li' che
; "ret" sopra salta), e subito sopra, sullo stack, l'entry point del task.
; Qui lo togliamo dallo stack e lo passiamo come primo argomento (rdi) a
; task_trampoline_c, seguendo la System V AMD64 calling convention.
global task_bootstrap_trampoline
extern task_trampoline_c
task_bootstrap_trampoline:
    pop rdi                  ; rdi = entry function pointer
    call task_trampoline_c
    ; task_trampoline_c chiama task_exit() internamente e non ritorna mai.
.unreachable:
    cli
    hlt
    jmp .unreachable

section .note.GNU-stack noalloc noexec nowrite progbits
