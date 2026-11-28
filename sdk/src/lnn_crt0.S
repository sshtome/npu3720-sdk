/*
 * lnn_crt0.S — LeonNN (Core 1 / %asr17[31:28]=2) entry stub and trap table.
 *
 * Requirements:
 * - 4 KB aligned entry (.section .text.lnn.entry)
 * - Separate stack (0x87FE0000)
 * - Soft-float only (no FPU instructions, PSR.EF=0)
 * - PSR: PIL=15, S=1, ET=1, CWP=0 (0x0FA0)
 * - WIM: 2 (window 1 invalid)
 * - Do NOT touch L2C (0x27000000) or erase common .bss
 */
    .section .text.lnn.entry, "ax"
    .align 4096
    .global npu_lnn_start_entry
    .type npu_lnn_start_entry, %function

npu_lnn_start_entry:
lnn_trap_table:
    /* Slot 0: Reset / Entry */
    b       lnn_start_code
    nop
    nop
    nop

    /* Slots 1..4 */
    .rept 4
    ta      0
    nop
    nop
    nop
    .endr

    /* Slot 5: window_overflow (NWINDOWS=8) */
    sethi   %hi(lnn_window_overflow), %l0
    jmp     %l0 + %lo(lnn_window_overflow)
    nop
    nop

    /* Slot 6: window_underflow (NWINDOWS=8) */
    sethi   %hi(lnn_window_underflow), %l0
    jmp     %l0 + %lo(lnn_window_underflow)
    nop
    nop

    /* Slots 7..255: default ta 0 */
    .rept 249
    ta      0
    nop
    nop
    nop
    .endr

    .section .text.lnn, "ax"
    .align 4

/* ---- window overflow (trap 0x05, NWINDOWS=8) ---- */
    .global lnn_window_overflow
    .type lnn_window_overflow, %function
lnn_window_overflow:
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
    std     %l0, [%sp + 0x00]
    std     %l2, [%sp + 0x08]
    std     %l4, [%sp + 0x10]
    std     %l6, [%sp + 0x18]
    std     %i0, [%sp + 0x20]
    std     %i2, [%sp + 0x28]
    std     %i4, [%sp + 0x30]
    std     %i6, [%sp + 0x38]
    restore
    mov     %l5, %g1
    jmp     %l1
    rett    %l2

/* ---- window underflow (trap 0x06, NWINDOWS=8) ---- */
    .global lnn_window_underflow
    .type lnn_window_underflow, %function
lnn_window_underflow:
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
    wr      %g0, %g1, %wim
    nop
    nop
    nop
    ldd     [%sp + 0x00], %l0
    ldd     [%sp + 0x08], %l2
    ldd     [%sp + 0x10], %l4
    ldd     [%sp + 0x18], %l6
    ldd     [%sp + 0x20], %i0
    ldd     [%sp + 0x28], %i2
    ldd     [%sp + 0x30], %i4
    ldd     [%sp + 0x38], %i6
    save
    save
    mov     %l5, %g1
    jmp     %l1
    rett    %l2

    .global lnn_start_code
    .type lnn_start_code, %function
lnn_start_code:
    /* 1) Force known PSR state: PIL=15, S=1, ET=0, EF=0, CWP=0 */
    set     0x0F80, %g1
    wr      %g0, %g1, %psr
    nop
    nop
    nop

    /* 2) Flush I-cache (0x81D82000) */
    .word   0x81D82000
    nop
    nop
    nop

    /* 3) Set %tbr = lnn_trap_table */
    set     lnn_trap_table, %g1
    wr      %g1, %tbr
    nop
    nop
    nop

    /* 4) Stack for LeonNN (0x87FE0000) */
    set     0x87FE0000, %sp
    mov     %sp, %fp

    /* 5) %wim = 2 (NWINDOWS=8, CWP=0 -> window 1 invalid) */
    wr      %g0, 2, %wim
    nop
    nop
    nop

    /* 6) Enable synchronous traps: PIL=15, S=1, ET=1, EF=0, CWP=0 */
    set     0x0FA0, %g1
    wr      %g0, %g1, %psr
    nop
    nop
    nop

    /* 7) Jump to C entry for LeonNN */
    call    npu_lnn_main
    nop

1:
    ba      1b
    nop
