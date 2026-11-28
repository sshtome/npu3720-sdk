/*
 * crt0.S — LeonRT (SPARC v8 LE) startup, trap table, window handlers (N=8),
 * fault_skip (trap 0x09), and crash_handler.
 */
    .text
    .section .text.entry
trap_table:
    .include "traps_table.S"

    .text
    .global _start
    .type _start, %function
_start:
    /* 0) Force known PSR state first: PIL=15 (mask IRQs), S=1, ET=0, CWP=0 */
    set     0x0F80, %g1
    wr      %g0, %g1, %psr
    nop
    nop
    nop

    /* 1) Disable watchdog */
    set     0x260200A4, %g1
    st      %g0, [%g1]

    /* 2) Flush I-cache (0x81D82000) */
    .word   0x81D82000
    nop
    nop
    nop

    /* 3) Stack (in CWP=0) */
    set     _stack_top, %sp
    mov     %sp, %fp

    /* 4) Zero .bss */
    set     __bss_start, %g1
    set     __bss_end, %g2
1:
    cmp     %g1, %g2
    bge     2f
    st      %g0, [%g1]
    add     %g1, 4, %g1
    ba      1b
    nop
2:
    /* 5) Set %tbr = trap_table (0x84803000) */
    set     trap_table, %g1
    wr      %g1, %tbr
    nop
    nop
    nop

    /* 6) %wim = 2 (NWINDOWS=8, CWP=0 -> window 1 invalid) */
    wr      %g0, 2, %wim
    nop
    nop
    nop

    /* 7) Enable synchronous traps & FPU: PIL=15, S=1, PS=0, ET=1, EF=1, CWP=0 */
    set     0x1FA0, %g1
    wr      %g0, %g1, %psr
    nop
    nop
    nop

    /* Clear %fsr (Floating-point State Register) */
    st      %g0, [%sp - 4]
    ld      [%sp - 4], %fsr
    nop
    nop
    nop

    call    kmain
    nop

1:
    ba      1b
    nop

/* ---- window overflow (trap 0x05, NWINDOWS=8) ---- */
    .text
    .type window_overflow, %function
window_overflow:
    mov     %g1, %l5
    mov     %wim, %l3
    sll     %l3, 7, %l4
    srl     %l3, 1, %l3
    or      %l3, %l4, %g1
    and     %g1, 0xFF, %g1
    wr      %g0, %g0, %wim
    nop
    nop
    nop
    save
    wr      %g0, %g1, %wim
    nop
    nop
    nop
    std     %l0, [%sp + 0]
    std     %l2, [%sp + 8]
    std     %l4, [%sp + 16]
    std     %l6, [%sp + 24]
    std     %i0, [%sp + 32]
    std     %i2, [%sp + 40]
    std     %i4, [%sp + 48]
    std     %i6, [%sp + 56]
    restore
    mov     %l5, %g1
    jmp     %l1
    rett    %l2

/* ---- window underflow (trap 0x06, NWINDOWS=8) ---- */
    .text
    .type window_underflow, %function
window_underflow:
    mov     %g1, %l5
    mov     %wim, %l3
    sll     %l3, 1, %l4
    srl     %l3, 7, %l3
    or      %l3, %l4, %g1
    and     %g1, 0xFF, %g1
    wr      %g0, %g0, %wim
    nop
    nop
    nop
    restore
    restore
    ldd     [%sp + 0], %l0
    ldd     [%sp + 8], %l2
    ldd     [%sp + 16], %l4
    ldd     [%sp + 24], %l6
    ldd     [%sp + 32], %i0
    ldd     [%sp + 40], %i2
    ldd     [%sp + 48], %i4
    ldd     [%sp + 56], %i6
    save
    save
    wr      %g0, %g1, %wim
    nop
    nop
    nop
    mov     %l5, %g1
    jmp     %l1
    rett    %l2

/* ---- data access exception (trap 0x09): record flag & skip faulting insn ---- */
    .text
    .type fault_skip, %function
fault_skip:
    sethi   %hi(g_fault_skipped), %l3
    mov     1, %l4
    st      %l4, [%l3 + %lo(g_fault_skipped)]
    jmp     %l2
    rett    %l2 + 4

/* ---- software trap ta 0x10 (tt = 0x90): syscall / yield ---- */
    .text
    .global syscall_trap_handler
    .type syscall_trap_handler, %function
syscall_trap_handler:
    sethi   %hi(g_syscall_cnt), %l3
    ld      [%l3 + %lo(g_syscall_cnt)], %l4
    add     %l4, 1, %l4
    st      %l4, [%l3 + %lo(g_syscall_cnt)]
    set     0x4F53, %i0     /* return "OS" magic in caller's %o0 */
    jmp     %l2
    rett    %l2 + 4

    .global g_syscall_cnt
    .section .bss
    .align 4
g_syscall_cnt:
    .word 0

/* ---- hardware interrupt trampoline (traps 0x11..0x1F, IRL 1..15) ---- */
    .text
    .type irq_trampoline, %function
irq_trampoline:
    sub     %fp, 144, %sp
    rd      %psr, %l0
    rd      %y, %l4
    st      %l0, [%sp + 96]
    st      %l4, [%sp + 100]
    st      %g1, [%sp + 104]
    st      %g2, [%sp + 108]
    st      %g3, [%sp + 112]
    st      %g4, [%sp + 116]
    st      %g5, [%sp + 120]
    st      %g6, [%sp + 124]
    st      %g7, [%sp + 128]

    /* If trap window itself is invalid in %wim, spill (CWP_trap - 1) mod 8 */
    and     %l0, 0x1F, %l4
    mov     1, %l5
    sll     %l5, %l4, %l5
    rd      %wim, %l6
    andcc   %l5, %l6, %g0
    be      1f
    nop

    sll     %l6, 7, %l4
    srl     %l6, 1, %l6
    or      %l6, %l4, %l6
    and     %l6, 0xFF, %l6
    wr      %g0, %g0, %wim
    nop
    nop
    nop
    save
    std     %l0, [%sp + 0]
    std     %l2, [%sp + 8]
    std     %l4, [%sp + 16]
    std     %l6, [%sp + 24]
    std     %i0, [%sp + 32]
    std     %i2, [%sp + 40]
    std     %i4, [%sp + 48]
    std     %i6, [%sp + 56]
    restore
    wr      %g0, %l6, %wim
    nop
    nop
    nop

1:
    /* Mask nested external IRQs (PIL=15) and enable traps (ET=1) for C handler */
    or      %l0, 0xF20, %l4
    wr      %g0, %l4, %psr
    nop
    nop
    nop

    call    npu_irq_dispatch
    mov     %l3, %o0

    /* Disable traps (ET=0, PIL=15) safely while adjusting window/regs */
    ld      [%sp + 96], %l0
    or      %l0, 0xF00, %l4
    wr      %g0, %l4, %psr
    nop
    nop
    nop

    ld      [%sp + 100], %l4
    wr      %g0, %l4, %y
    ld      [%sp + 104], %g1
    ld      [%sp + 108], %g2
    ld      [%sp + 112], %g3
    ld      [%sp + 116], %g4
    ld      [%sp + 120], %g5
    ld      [%sp + 124], %g6
    ld      [%sp + 128], %g7

    /* Ensure return window (CWP_trap + 1) mod 8 is valid before rett */
    and     %l0, 0x1F, %l4
    add     %l4, 1, %l4
    and     %l4, 7, %l4
    mov     1, %l5
    sll     %l5, %l4, %l5
    rd      %wim, %l6
    andcc   %l5, %l6, %g0
    be      2f
    nop

    sll     %l6, 1, %l4
    srl     %l6, 7, %l6
    or      %l6, %l4, %l6
    and     %l6, 0xFF, %l6
    wr      %g0, %g0, %wim
    nop
    nop
    nop
    restore
    ldd     [%sp + 0], %l0
    ldd     [%sp + 8], %l2
    ldd     [%sp + 16], %l4
    ldd     [%sp + 24], %l6
    ldd     [%sp + 32], %i0
    ldd     [%sp + 40], %i2
    ldd     [%sp + 48], %i4
    ldd     [%sp + 56], %i6
    save
    wr      %g0, %l6, %wim
    nop
    nop
    nop

2:
    /* Restore exact pre-trap PSR (PIL, icc, PS, CWP_trap, ET=0) AFTER andcc */
    wr      %g0, %l0, %psr
    nop
    nop
    nop
    jmp     %l1
    rett    %l2

/* ---- crash_handler: dump trap state to fw_log @ 0x90000150 and halt ---- */
    .text
    .type crash_handler, %function
crash_handler:
    set     0x90000150, %g1
    st      %g2, [%g1 + 0]
    st      %l1, [%g1 + 4]
    st      %l2, [%g1 + 8]
    rd      %wim, %g3
    st      %g3, [%g1 + 12]
    rd      %psr, %g3
    st      %g3, [%g1 + 16]
    mov     %sp, %g3
    st      %g3, [%g1 + 20]
    set     0x90000044, %g3
    mov     0x200, %g1
    st      %g1, [%g3]
1:
    ba      1b
    nop

    .bss
    .global g_fault_skipped
g_fault_skipped:
    .skip 4
