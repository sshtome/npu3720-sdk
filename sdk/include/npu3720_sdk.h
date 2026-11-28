/*
 * npu3720_sdk.h — Public bare-metal C API for Intel NPU 3720 (LeonRT SPARC v8 LE).
 *
 * Hardware Invariants & Architectural Constraints:
 * - Architecture: SPARC v8 32-bit Little-Endian (sparcel).
 * - Register Windows: NWINDOWS = 8 (%asr17[4:0] = 7).
 * - Trap Handling: PSR.ET must be 1 before subroutine calls or traps.
 *   Synchronous traps with ET=0 cause an immediate processor reset into Error Mode.
 * - Interrupts: Async traps (0x11..0x1F) enter with ET=0; trampoline must spill
 *   if the trap window matches %wim, set PIL=15, and enable ET=1 before C dispatch.
 * - PIL Restoration: npu_irq_restore() must RMW ONLY bits [11:8] of %psr to preserve CWP.
 * - Multicore: Core 0 (LeonRT) has a hardware FPU (requires %fsr clear on boot);
 *   Core 1 (LeonNN) has NO FPU and must be compiled with -msoft-float (PSR.EF=0).
 * - LeonNN Alignment: Entry point and trap table must be 4 KB aligned (0x1000).
 * - NNDMA / SMMU: HOST_IF_SSID_MAP (0x20340000) entries must be 0 for Global Context (SSID 0).
 * - Cache Coherency: DDR DMA transfers require explicit L1 D-cache flush (ASI 0x11)
 *   and GRLIB L2C line clean/flush (0x27000008).
 */
#ifndef NPU3720_SDK_H
#define NPU3720_SDK_H

#include "npu3720_hw.h"

/* ============================================================================
 * 1. Lifecycle & Polling
 * ========================================================================== */

/**
 * @brief Initialize all core SDK hardware subsystems.
 *
 * Configures GRLIB L2C MTRR (0x9000FFE1 for uncached shared DDR), initializes
 * the coherent fw_log buffer at 0x90000000, arms TIM_PERF_FREE_CNT (38.4 MHz),
 * sets up Host<->Device IPC ring buffers, and transmits the "BOOT" IPC handshake.
 *
 * @param fw_name Optional firmware name string displayed in host fw_log.
 */
void npu_init(const char *fw_name);

/**
 * @brief Main event polling loop iteration.
 *
 * Checks H2D FIFO for incoming host console characters (0xCCxxxxxx), LeonNN IPC
 * messages (0xBBxxxxxx), and JSM requests (0x1113 Priority Band, 0x1104 Heartbeat).
 * Must be called periodically to satisfy host driver watchdog requirements.
 */
void npu_poll(void);

/* ============================================================================
 * 2. Cache & GRLIB L2C Controller (0x27000000)
 * ========================================================================== */

/**
 * @brief Read L1 Cache Control Register (CCR) via SPARC ASI 0x02.
 * @return 32-bit CCR value (bit 23 = DS data snooping, bits [3:0] = cache enables).
 */
u32  npu_l1_ccr_read(void);

/**
 * @brief Write L1 Cache Control Register (CCR) via SPARC ASI 0x02.
 * @param val CCR control word.
 */
void npu_l1_ccr_write(u32 val);

/**
 * @brief Return initial boot value of L1 CCR before SDK reconfiguration.
 */
u32  npu_l1_ccr_initial(void);

/**
 * @brief Flush entire L1 data cache using SPARC ASI 0x11 (sta %g0, [%g0] 0x11).
 * Invariant: ASI 0x11 is hardware-accelerated on LeonRT; ASI 0x0F does not flush L2.
 */
void npu_l1_dcache_flush(void);

/**
 * @brief Enable or disable automated L1 cache flushes during log and IPC writes.
 * @param en 1 to enable, 0 to disable.
 */
void npu_l1_set_flush_enabled(int en);

/**
 * @brief Check whether automated L1 cache flushes are enabled.
 * @return 1 if enabled, 0 if disabled.
 */
int  npu_l1_is_flush_enabled(void);

/**
 * @brief Clean (writeback) a range of physical memory from GRLIB L2 Cache.
 * @param addr Starting physical byte address.
 * @param size Number of bytes to clean.
 */
void npu_l2c_clean_range(u32 addr, u32 size);

/**
 * @brief Flush and invalidate all lines in both L1 data cache and GRLIB L2 Cache.
 */
void npu_l2c_flush_all(void);

/**
 * @brief Program a GRLIB L2C Memory Type Range Register (MTRR).
 * @param idx MTRR register index (0..n_mtrr-1).
 * @param addr Base address (aligned to range size).
 * @param mask Address mask bits [15:2].
 * @param access_ctrl Access control (0 = uncached, 1 = cached, etc.).
 * @param wp Write-protect flag (bit 1).
 * @param enable 1 to enable MTRR entry, 0 to disable.
 */
void npu_l2c_set_mtrr(u32 idx, u32 addr, u32 mask, u32 access_ctrl, int wp, int enable);

/**
 * @brief Configure MTRR[0] for uncached shared host DDR region (0x90000000..0x90200000).
 * Enables coherent fw_log and IPC without manual L2C flushes on every write.
 * @return 0 on success, -1 if L2C reports 0 available MTRRs.
 */
int  npu_l2c_configure_shared_uncached(void);

/**
 * @brief Check if GRLIB L2C MTRR uncached mapping is currently active.
 * @return 1 if active, 0 otherwise.
 */
int  npu_l2c_is_mtrr_active(void);

/**
 * @brief Return initial boot value of L2C_CTRL (0x27000000).
 */
u32  npu_l2c_ctrl_initial(void);

/**
 * @brief Return initial boot value of L2C_STAT (0x27000004).
 */
u32  npu_l2c_stat_initial(void);

/* ============================================================================
 * 3. Logging (fw_log @ 0x90000000)
 * ========================================================================== */

/**
 * @brief Initialize circular tracing buffer in shared DDR (512 KB @ 0x90000000).
 * Host reads this via /sys/kernel/debug/accel/0000:00:0b.0/fw_log.
 * @param name Log channel name (up to 15 characters).
 */
void npu_log_init(const char *name);

/**
 * @brief Append a single character to fw_log buffer.
 * @param c Character to append.
 */
void npu_log_putc(char c);

/**
 * @brief Append a null-terminated string to fw_log buffer and trigger flush.
 * @param s String to write.
 */
void npu_log_puts(const char *s);

/**
 * @brief Write 32-bit unsigned integer as an 8-character uppercase hex string.
 * @param v Value to write.
 */
void npu_log_hex(u32 v);

/**
 * @brief Write 32-bit unsigned integer as a decimal string.
 * @param v Value to write.
 */
void npu_log_dec(u32 v);

/**
 * @brief Flush log write pointer and ensure L1/L2 cache coherency with host.
 */
void npu_log_flush(void);

/* ============================================================================
 * 4. IPC & JSM (Job Scheduler Manager)
 * ========================================================================== */

/**
 * @brief Initialize IPC ring buffer pointers from boot_params (0x84800000).
 */
void npu_ipc_init(void);

/**
 * @brief Enable hardware interrupt for H2D FIFO incoming messages (IRQ 12).
 */
void npu_ipc_enable_irq(void);

/**
 * @brief Transmit an IPC descriptor to the host via D2H FIFO (0x200200F0).
 * @param data_addr Physical address of payload buffer.
 * @param data_size Size of payload in bytes.
 * @param channel IPC channel ID.
 */
void npu_ipc_send(u32 data_addr, u32 data_size, u16 channel);

/**
 * @brief Send initial "BOOT" IPC handshake message (channel 0x3FF).
 */
void npu_ipc_send_boot(void);

/**
 * @brief Poll H2D FIFO and process pending JSM / console / inter-core messages.
 */
void npu_jsm_poll(void);

/**
 * @brief Return total count of JSM messages processed since boot.
 */
u32  npu_jsm_count(void);

/**
 * @brief Return type ID of the most recently processed JSM message.
 */
u32  npu_jsm_last_type(void);

/* ============================================================================
 * 5. High-Resolution Timer & Monotonic Clock
 * ========================================================================== */

/**
 * @brief Initialize and start 38.4 MHz TIM_PERF_FREE_CNT (0x26029000/04).
 */
void npu_timer_init(void);

/**
 * @brief Read lower 32 bits of monotonic performance counter.
 * @return Counter ticks (38.4 ticks per microsecond).
 */
u32  npu_ticks(void);

/**
 * @brief Atomically sample full 64-bit performance counter via SPARC 'ldd'.
 * Invariant: 64-bit atomic read ensures no race between high and low words.
 * @param lo Output pointer for low 32 bits.
 * @param hi Output pointer for high 32 bits.
 */
void npu_ticks64(u32 *lo, u32 *hi);

/**
 * @brief Return calibrated timer frequency in Hertz (nominal 38,400,000 Hz).
 */
u32  npu_timer_freq_hz(void);

/**
 * @brief Return monotonic system uptime in milliseconds since timer init.
 * Handles 32-bit counter rollover without 64-bit software division.
 */
u32  npu_uptime_ms(void);

/**
 * @brief Configure and arm Timer0 (0x2602800C) for periodic ticks (IRQ 108).
 * @param period_us Periodic interval in microseconds.
 */
void npu_timer_tick_start(u32 period_us);

/**
 * @brief Acknowledge Timer0 tick interrupt by clearing status bit.
 */
void npu_timer_tick_ack(void);

/* ============================================================================
 * 6. Interrupt Control Block (ICB @ 0x26000000 / 0x2F000000)
 * ========================================================================== */

typedef void (*npu_irq_handler_t)(u32 irq, void *ctx);

/**
 * @brief Initialize ICB hardware: mask all 128 IRQs, clear latches, drain ACKs.
 */
void npu_irq_init(void);

/**
 * @brief Register a C handler callback for an IRQ line.
 * @param irq IRQ number (0..127).
 * @param handler Function pointer to callback.
 * @param ctx Arbitrary user context pointer passed to handler.
 */
void npu_irq_register(u32 irq, npu_irq_handler_t handler, void *ctx);

/**
 * @brief Configure priority/edge mode and unmask an IRQ line in ICB.
 * @param irq IRQ number (0..127).
 * @param prio Priority level (0..15).
 * @param edge 1 for edge-triggered, 0 for level-sensitive.
 */
void npu_irq_enable(u32 irq, u32 prio, int edge);

/**
 * @brief Mask an IRQ line in ICB.
 * @param irq IRQ number (0..127).
 */
void npu_irq_disable(u32 irq);

/**
 * @brief Clear a pending latched IRQ in ICB (W1C acknowledge).
 * @param irq IRQ number (0..127).
 */
void npu_irq_clear(u32 irq);

/**
 * @brief Force an interrupt via software ICB_FORCE register (useful for IPI).
 * @param icb_base Base address of ICB (0 for LeonRT 0x26000000, or 0x2F000000 for LeonNN).
 * @param irq IRQ number (0..127).
 */
void npu_irq_force(u32 icb_base, u32 irq);

/**
 * @brief Return total number of times an IRQ line has been serviced.
 * @param irq IRQ number (0..127).
 */
u32  npu_irq_count(u32 irq);

/**
 * @brief Return count of spurious (unrecognized / unlatched) interrupts.
 */
u32  npu_irq_spurious_count(void);

/**
 * @brief Globally enable external interrupts by setting PSR.PIL = 0.
 */
void npu_irq_global_enable(void);

/**
 * @brief Globally disable external interrupts by setting PSR.PIL = 15.
 */
void npu_irq_global_disable(void);

/**
 * @brief Save current %psr and mask external interrupts (PIL = 15).
 * @return Raw 32-bit %psr register value at time of call.
 */
u32  npu_irq_save(void);

/**
 * @brief Restore PIL level from saved %psr.
 * Invariant: Modifies ONLY bits [11:8] (PIL) of %psr. Must never overwrite
 * CWP bits [4:0], otherwise register window corruption occurs.
 * @param psr Saved %psr value from npu_irq_save().
 */
void npu_irq_restore(u32 psr);

/* ============================================================================
 * 7. OS Primitives: Syscall Trap & SPARC Hardware Atomics
 * ========================================================================== */

/**
 * @brief Issue software trap ta 0x10 (tt = 0x90) for OS yield / syscall dispatch.
 * @return Integer return code in caller's %o0 (default handler returns 0x4F53 "OS").
 */
u32  npu_syscall_yield(void);

/**
 * @brief Return total count of handled software syscall traps.
 */
u32  npu_get_syscall_count(void);

/**
 * @brief Execute SPARC v8 atomic 32-bit swap instruction.
 * Atomically exchanges *addr with val. Verified on DDR and NNCMX SRAM.
 * @param addr 32-bit aligned memory address.
 * @param val Value to write.
 * @return Previous 32-bit value stored at *addr.
 */
static inline u32 npu_atomic_swap(volatile u32 *addr, u32 val)
{
    __asm__ volatile("swap [%1], %0" : "+r"(val) : "r"(addr) : "memory");
    return val;
}

/**
 * @brief Execute SPARC v8 atomic 8-bit load-and-store-unsigned-byte (ldstub).
 * Reads byte from *addr, writes 0xFF to *addr atomically. Ideal for spinlocks.
 * Verified on DDR and NNCMX SRAM.
 * @param addr Byte memory address.
 * @return Previous byte value stored at *addr (0 = lock acquired, 0xFF = busy).
 */
static inline u8 npu_atomic_ldstub(volatile u8 *addr)
{
    u32 val = 0;
    __asm__ volatile("ldstub [%1], %0" : "=r"(val) : "r"(addr) : "memory");
    return (u8)val;
}

/* ============================================================================
 * 8. SPARC Reference MMU (SRMMU, ASI 0x19 / ASI 0x18)
 * ========================================================================== */

/**
 * @brief Configure 256-context table and 1-level 16 MB section identity mapping.
 * Maps MMIO & NNCMX uncached (0x7E), DDR cached (0xFE), Shared DDR uncached (0x7E),
 * and adds test virtual alias VA 0x10000000 -> PA 0x2E000000.
 */
void npu_mmu_init_identity(void);

/**
 * @brief Flush TLB (ASI 0x18) and enable SRMMU translation (ASI 0x19 CTRL bit 0).
 */
void npu_mmu_enable(void);

/**
 * @brief Disable SRMMU translation and flush TLB.
 */
void npu_mmu_disable(void);

/**
 * @brief Read MMU Control Register via ASI 0x19 offset 0x000.
 */
u32  npu_mmu_read_ctrl(void);

/**
 * @brief Read Synchronous Fault Status Register via ASI 0x19 offset 0x300.
 */
u32  npu_mmu_read_sfsr(void);

/**
 * @brief Read Synchronous Fault Address Register via ASI 0x19 offset 0x400.
 */
u32  npu_mmu_read_sfar(void);

/* ============================================================================
 * 9. Fault Recovery & Safe Memory Access
 * ========================================================================== */

/**
 * @brief Safely attempt a 32-bit MMIO read guarded by trap 0x09 (fault_skip).
 * If the read triggers an AHB data access exception (< 0x80000000), trap 0x09 skips
 * the faulting instruction and sets g_fault_skipped, preventing a processor hang.
 * Note: Avoid reading 0xA0000000+ which triggers a host PCIe TBU SMMU fault.
 * @param addr Physical address to read.
 * @param out Pointer to store read value on success.
 * @return 0 on successful read, -1 if trap 0x09 caught a bus fault.
 */
int  npu_safe_read32(u32 addr, u32 *out);

/* ============================================================================
 * 10. Power, Clocks & Reset (CPR / PUnit / AON)
 * ========================================================================== */

/**
 * @brief Power on NCE Spine (AON Island 5), unmask TOP_NOC AXI, and bring up NCE NoC.
 * Required before accessing NNCMX SRAM, LeonNN Core 1, or NNDMA.
 * @return 0 on success, negative error code on timeout.
 */
int  npu_cpr_init_nce_spine(void);

/**
 * @brief Power on Tile 0 (AON Island 32, slices 0-1) and pulse TILE_AUX_SET.
 * @return 0 on success, negative error code on timeout.
 */
int  npu_cpr_power_tile0(void);

/**
 * @brief Power on Tile 1 (AON Island 33, slices 2-3) and pulse TILE_AUX_SET.
 * @return 0 on success, negative error code on timeout.
 */
int  npu_cpr_power_tile1(void);

/**
 * @brief Sequentially power up NCE Spine, Tile 0, and Tile 1.
 * @return 0 on success, negative error code on failure.
 */
int  npu_cpr_power_compute_all(void);

/**
 * @brief Read PUnit active workpoint and PLL ratio mirror (0x20082020).
 * @return 32-bit workpoint status (e.g. 0x02020016 for PLL ratio 22).
 */
u32  npu_cpr_punit_status(void);

/* ============================================================================
 * 11. NNCMX SRAM (0x2E000000..0x2E3FFFFF, 4 MB Total)
 * ========================================================================== */

/**
 * @brief Probe accessibility of Tile 0 (0x2E000000) and Tile 1 (0x2E200000).
 * @return 0 if both tiles accessible, -1 if Tile 0 faults, -2 if Tile 1 faults.
 */
int  npu_cmx_probe(void);

/**
 * @brief Verify bit pattern write/read integrity across all 4 SRAM slices without aliasing.
 * @return 0 on success, negative error code identifying failed offset.
 */
int  npu_cmx_test_rw_integrity(void);

/**
 * @brief Measure sequential write/read throughput across 64 KB blocks.
 * Typical results: CMX write ~262 MB/s, CMX read ~30 MB/s, DDR write ~18 MB/s.
 * @param cmx_write_mbs Output MB/s for CMX write.
 * @param cmx_read_mbs Output MB/s for CMX read.
 * @param ddr_write_mbs Output MB/s for DDR write.
 * @param ddr_read_mbs Output MB/s for DDR read.
 */
void npu_cmx_benchmark(u32 *cmx_write_mbs, u32 *cmx_read_mbs, u32 *ddr_write_mbs, u32 *ddr_read_mbs);

/* ============================================================================
 * 12. LeonNN (Core 1, %asr17[31:28] == 2) Multicore Management
 * ========================================================================== */

/**
 * @brief Shared inter-core mailbox located at NNCMX Tile 0 base (0x2E000000).
 */
struct npu_lnn_mailbox {
    volatile u32 magic;       /**< Magic identifier: 0x4C4E4E31 ("LNN1") */
    volatile u32 asr17;       /**< Core 1 %asr17 (reads 0x22E00187, Core ID = 2) */
    volatile u32 counter;     /**< Heartbeat counter incremented by Core 1 */
    volatile u32 status;      /**< Lifecycle status: 1 = booted/ready, 2 = stopping */
    volatile u32 last_rx_ipc; /**< Last word received via TIM_IPC_FIFO */
    volatile u32 srmmu_ctrl;  /**< Core 1 ASI 0x19 0x000 (0x00800000 on silicon) */
    volatile u32 ccr;         /**< Core 1 ASI 0x02 0x000 (0x00020000 on silicon) */
    volatile u32 fault_flag;  /**< Bit 0: SRMMU faulted, Bit 1: CCR faulted */
};

extern void npu_lnn_start_entry(void);

/**
 * @brief Launch secondary core (LeonNN) at entry_addr.
 * Invariants:
 * - entry_addr must be 4 KB aligned ((entry_addr & 0xFFF) == 0).
 * - Code executed by Core 1 must be compiled with -msoft-float (no FPU).
 * - Shared GRLIB L2C must not be modified by Core 1.
 * @param entry_addr Physical address of Core 1 entry point & trap table.
 * @return 0 on success, -1 on invalid alignment, -2 on CPR failure.
 */
int  npu_leonnn_start(u32 entry_addr);

/**
 * @brief Signal LeonNN Core 1 to stop execution and clear RUN bit in 0x2F008000.
 */
void npu_leonnn_stop(void);

/**
 * @brief Check whether LeonNN Core 1 is actively running.
 * @return 1 if running, 0 if stopped.
 */
int  npu_leonnn_is_running(void);

/**
 * @brief Return pointer to shared NNCMX mailbox structure (0x2E000000).
 */
struct npu_lnn_mailbox *npu_leonnn_get_mailbox(void);

/**
 * @brief Send 32-bit IPC word from LeonRT to LeonNN via FIFO (0x2F0020F0).
 * @param word Message word.
 */
void npu_leonnn_send_ipc(u32 word);

/**
 * @brief Handler called when an inter-core IPC word is received from LeonNN.
 * @param word Message word tagged with 0xBB000000.
 */
void npu_lnn_on_ipc_word(u32 word);

/**
 * @brief Return total number of IPC words received from LeonNN.
 */
u32  npu_lnn_get_rx_ipc_count(void);

/**
 * @brief Return last IPC word received from LeonNN.
 */
u32  npu_lnn_get_last_rx_ipc_word(void);

/* ============================================================================
 * 13. Interactive Console
 * ========================================================================== */

typedef void (*npu_console_cb_t)(const char *line);

/**
 * @brief Register a line callback for host interactive console.
 * Host sends ASCII characters encoded into bits [21:6] of 0xCCxxxxxx words.
 * @param cb Callback function receiving null-terminated line strings.
 */
void npu_console_register(npu_console_cb_t cb);

/**
 * @brief Process incoming console 32-bit word from H2D FIFO.
 * Extracts 2 ASCII bytes from bits [21:6] and pushes to line buffer.
 * @param word Raw 0xCCxxxxxx word.
 */
void npu_console_rx_word(u32 word);

/* ============================================================================
 * 14. NNDMA Hardware Engine (Tile 0 & Tile 1)
 * ========================================================================== */

/**
 * @brief Initialize NNDMA subsystem:
 * Un-gates CPR Domain 2 clocks/resets (mask 0xC00), enables engines and decoders,
 * programs NOC arbitration (0x2F004000/6000), and clears all 32 entries of
 * HOST_IF_SSID_MAP (0x20340000) to 0 (SSID 0 = Global Context) to prevent SMMU faults.
 */
void npu_dma_init(void);

/**
 * @brief Execute a synchronous 1D DMA transfer between DDR and NNCMX SRAM.
 * Places a 64-byte descriptor in NNCMX pool (0x2E008000), flushes L1D/L2C if source
 * is in DDR, kicks Link Agent 0 via 64-bit atomic store 'std' to engine+0x640,
 * and polls completion status at engine+0x840.
 *
 * @param tile Compute tile index (0 or 1).
 * @param dst Destination physical address.
 * @param src Source physical address.
 * @param len Byte count to transfer.
 * @return 0 on success, -1 on timeout.
 */
int  npu_dma_copy_1d(unsigned tile, u32 dst, u32 src, u32 len);

#endif /* NPU3720_SDK_H */
