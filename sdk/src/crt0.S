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
    /* 1) Disable watchdog */
    set     0x260200A4, %g1
    st      %g0, [%g1]

    /* 2) Flush I-cache (0x81D82000) */
    .word   0x81D82000
    nop
    nop
    nop

    /* 3) Stack */
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

    call    kmain
    nop

1:
    ba      1b
    nop

/* ---- window overflow (trap 0x05, NWINDOWS=8) ---- */
    .text
    .type window_overflow, %function
window_overflow:
    mov     %wim, %l3
    sll     %l3, 7, %l4
    srl     %l3, 1, %l3
    or      %l3, %l4, %l3
    save
    mov     %l3, %wim
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
    jmp     %l1
    rett    %l2

/* ---- window underflow (trap 0x06, NWINDOWS=8) ---- */
    .text
    .type window_underflow, %function
window_underflow:
    mov     %wim, %l3
    sll     %l3, 1, %l4
    srl     %l3, 7, %l3
    or      %l3, %l4, %l3
    mov     %l3, %wim
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
