.syntax unified
.cpu cortex-a9
.arm

@ IRQ entry. Based on examples/irq-context-switch/irq.S from the course template.
@ Builds an 88-byte struct minemu_trap_frame on the IRQ stack, calls the C
@ dispatcher, then restores whatever frame the dispatcher returns.

.global minemu_irq_trampoline
.type minemu_irq_trampoline, %function
.extern minemu_irq_dispatch

minemu_irq_trampoline:
    sub sp, sp, #88              @ allocate one trap frame on the IRQ stack
    stmia sp, {r0-r12}           @ save r0-r12          (offsets 0..48)
    str lr, [sp, #52]            @ save IRQ lr          (return_lr)
    mrs r1, spsr
    str r1, [sp, #56]            @ save interrupted CPSR (spsr)
    ldr r1, =0x10000000          @ interrupt controller base
    ldr r1, [r1, #8]             @ read CLAIM -> active source id
    str r1, [sp, #60]            @ exception_id
    sub r1, lr, #4
    str r1, [sp, #64]            @ fault_pc = interrupted instruction
    mov r1, #0
    str r1, [sp, #68]            @ dfsr (unused for IRQ)
    str r1, [sp, #72]            @ dfar (unused for IRQ)
    add r1, sp, #76
    stmia r1, {sp, lr}^          @ save interrupted mode's banked sp/lr
    mov r1, #0
    str r1, [sp, #84]            @ reserved
    mov r0, sp                   @ arg0 = frame
    bl minemu_irq_dispatch       @ r0 = frame to resume
    mov sp, r0
    ldr lr, [sp, #52]
    ldr r1, [sp, #56]
    msr spsr_cxsf, r1            @ restore interrupted CPSR on return
    add r1, sp, #76
    ldmia r1, {sp, lr}^          @ restore banked sp/lr
    ldmia sp, {r0-r12}
    add sp, sp, #88              @ pop the frame
    subs pc, lr, #4              @ return to interrupted instruction, CPSR <- SPSR
