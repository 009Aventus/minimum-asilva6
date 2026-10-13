.syntax unified
.cpu cortex-a9
@ Based on examples/user-mode-hello/kernel/svc.S from the course template.
.arm

.global minemu_svc_trampoline
.type minemu_svc_trampoline, %function
.extern minemu_svc_dispatch

minemu_svc_trampoline:
    sub sp, sp, #88
    stmia sp, {r0-r12}
    str lr, [sp, #52]
    mrs r1, spsr
    str r1, [sp, #56]     
    ldr r1, =-3                     @ what is -3? -3 is the code for "SVC exception". It's stored in exception_id so dispatcher can confirm why we entered the kernel 
    str r1, [sp, #60]
    sub r1, lr, #4		    @ What address is this? On a sys call, the cpu sets lr to the instruction after svc, so lr - 4 steps back to svc. Dispatcher reads this address to check instruction 
    str r1, [sp, #64]
    mov r1, #0
    str r1, [sp, #68]
    str r1, [sp, #72]
    add r1, sp, #76
    stmia r1, {sp, lr}^
    mov r1, #0
    str r1, [sp, #84]
    mov r0, sp
    bl minemu_svc_dispatch
    mov sp, r0
    ldr lr, [sp, #52]
    ldr r1, [sp, #56]
    msr spsr_cxsf, r1
    add r1, sp, #76
    ldmia r1, {sp, lr}^
    ldmia sp, {r0-r12}
    add sp, sp, #88
    movs pc, lr			    @ why no #4 here? Svc has already done its job, so we return to the next instruction. Return lr -4 would run the svc again and repeat sys call forever. 
