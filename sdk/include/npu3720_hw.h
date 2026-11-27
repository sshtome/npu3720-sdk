/*
 * npu3720_hw.h — hardware map for Intel NPU 3720 (LeonRT, SPARC v8 LE).
 *
 * Verification tags:
 *   [VERIFIED-HW]   — confirmed on real hardware.
 *   [VERIFIED-CODE] — cross-checked against firmware disassembly and linux-ivpu.
 *   [DERIVED]       — derived from +0x20000000 MMIO window rule or single source.
 */
#ifndef NPU3720_HW_H
#define NPU3720_HW_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  s32;

#define PACKED __attribute__((packed))

/* ============================================================================
 * 1. LeonRT Memory Map
 * ========================================================================== */

#define NPU_BOOT_PARAMS_ADDR      0x84800000u  /* [VERIFIED-HW] 4 KB */
#define NPU_FW_VERSION_ADDR       0x84801000u  /* [VERIFIED-HW] 4 KB */
#define NPU_FW_IMAGE_BASE         0x84802000u  /* [VERIFIED-HW] file +0x2000 */
#define NPU_FW_ENTRY              0x84803000u  /* [VERIFIED-HW] entry_point / default TBR */
#define NPU_FW_RUNTIME_SIZE       0x04000000u  /* [VERIFIED-CODE] 64 MB */
#define NPU_FW_IMAGE_MAX          0x88800000u  /* [VERIFIED-CODE] */
#define NPU_STACK_TOP             0x87FF0000u  /* [VERIFIED-HW] */

#define NPU_SHARED_REGION_BASE    0x90000000u  /* [VERIFIED-HW] 256 MB */
#define NPU_LOG_CRIT_BUF          0x90000000u  /* [VERIFIED-HW] 512 KB */
#define NPU_LOG_VERB_BUF          0x90080000u  /* [VERIFIED-CODE] 1 MB */
#define NPU_IPC_HEADER_AREA       0x90184000u  /* [VERIFIED-HW] 8 KB */
#define NPU_IPC_HEADER_AREA_SIZE  0x2000u      /* [VERIFIED-HW] */
#define NPU_IPC_PAYLOAD_AREA      0x90186000u  /* [VERIFIED-HW] 8 KB */
#define NPU_IPC_PAYLOAD_AREA_SIZE 0x2000u      /* [VERIFIED-HW] */
#define NPU_ALIASED_PIO_BASE      0xA0000000u  /* [VERIFIED-HW] unused; faults TBU */
#define NPU_HIGH_MEM_BASE         0x180000000ull /* [VERIFIED-HW] */

/* Internal peripheral MMIO window: CPU addr = host BAR0 + 0x20000000 */
#define NPU_WIN_SHIFT             0x20000000u

/* ============================================================================
 * 2. MMIO: HOST_SS (0x20000000 = host BAR0 + 0x00000000)
 * ========================================================================== */

#define NPU_HW_VERSION            (0x00000108u + NPU_WIN_SHIFT) /* [DERIVED] */
#define NPU_LOADING_ADDRESS_LO    (0x00041040u + NPU_WIN_SHIFT) /* [DERIVED] */
#define NPU_D2H_FIFO_WRITE        (0x000200F0u + NPU_WIN_SHIFT) /* [VERIFIED-HW] */
#define NPU_D2H_FIFO_ATM          (0x000200F4u + NPU_WIN_SHIFT) /* [DERIVED] */
#define NPU_D2H_FIFO_STAT         (0x000200FCu + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_D2H_FIFO_DOORBELL     (0x00020100u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */

#define NPU_D2H_STAT_RPTR_MASK    0x000000FFu
#define NPU_D2H_STAT_WPTR_MASK    0x0000FF00u
#define NPU_D2H_STAT_FILL_MASK    0x00FF0000u
#define NPU_D2H_DESC_SRC_ID_MASK  0x3Fu
#define NPU_D2H_DESC_SRC_ID_CORE0 5u           /* [VERIFIED-CODE] */

/* ============================================================================
 * 3. MMIO: CPU_SS (0x26000000 = host BAR0 + 0x06000000)
 * ========================================================================== */

#define NPU_IRQMP_PENDING         (0x06000230u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_CPR_LEON_RT_VEC       (0x06010040u + NPU_WIN_SHIFT) /* [DERIVED] */
#define NPU_H2D_FIFO_WRITE        (0x060200F0u + NPU_WIN_SHIFT) /* [VERIFIED-HW] */
#define NPU_H2D_FIFO_ATM          (0x060200F4u + NPU_WIN_SHIFT) /* [VERIFIED-HW] */
#define NPU_H2D_FIFO_STAT         (0x060200FCu + NPU_WIN_SHIFT) /* [VERIFIED-HW] */
#define NPU_H2D_FIFO_ACK          (0x06020220u + NPU_WIN_SHIFT) /* [VERIFIED-HW] */
#define NPU_H2D_POP_ACK_VAL       0x1000u                       /* [VERIFIED-HW] */
#define NPU_TIM_WATCHDOG          (0x0602009Cu + NPU_WIN_SHIFT) /* [VERIFIED-HW] */
#define NPU_TIM_WDOG_EN           (0x060200A4u + NPU_WIN_SHIFT) /* [VERIFIED-HW] */
#define NPU_TIM_SAFE              (0x060200A8u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_TIM_SAFE_UNLOCK_KEY   0xF1D0DEADu                   /* [VERIFIED-CODE] */
#define NPU_TIM_PRESCALER         (0x06021000u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_TIM_PRESCALER_RELOAD  (0x06021004u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_TIM_GEN_CONFIG        (0x06021008u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_TIM_PERF_FRC_EN       (0x06028000u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_TIM_TICK_CTRL         (0x0602800Cu + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_TIM_TICK_STAT         (0x06028010u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_TIM_TICK_RELOAD       (0x06028014u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_TIM_PERF_FREE_CNT_LO  (0x06029000u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_TIM_PERF_FREE_CNT_HI  (0x06029004u + NPU_WIN_SHIFT) /* [VERIFIED-CODE] */
#define NPU_TIM_DEFAULT_FREQ_HZ   38400000u                     /* [VERIFIED-CODE] */
#define NPU_DOORBELL_0            (0x06300000u + NPU_WIN_SHIFT) /* [DERIVED] */
#define NPU_CONSOLE_TAG           0xCC000000u                   /* [VERIFIED-HW] */

/* ============================================================================
 * 4. MMIO: GRLIB L2C (0x27000000) and Compute Windows
 * ========================================================================== */

#define NPU_GRLIB_L2C_BASE        0x27000000u /* [VERIFIED-HW/CODE] */
#define NPU_GRLIB_L2C_CTRL        (NPU_GRLIB_L2C_BASE + 0x00u)
#define NPU_GRLIB_L2C_STAT        (NPU_GRLIB_L2C_BASE + 0x04u)
#define NPU_GRLIB_L2C_FLUSH       (NPU_GRLIB_L2C_BASE + 0x08u)
#define NPU_GRLIB_L2C_MTRR(i)     (NPU_GRLIB_L2C_BASE + 0x80u + 4u * (i))

#define NPU_L2C_CTRL_EN           (1u << 31)
#define NPU_L2C_CTRL_ENABLE_VAL   0x80000001u /* [VERIFIED-CODE] */
#define NPU_L2C_MTRR_SHARED_UNCACHED 0x9000FFE1u /* 0x90000000..0x90200000, AC=0, EN=1 */
#define NPU_SHARED_UNCACHED_SIZE  0x00200000u

#define NPU_L2C_OP_INV_LINE       1u
#define NPU_L2C_OP_CLEAN_LINE     2u
#define NPU_L2C_OP_FLUSH_LINE     3u
#define NPU_L2C_OP_INV_ALL        5u
#define NPU_L2C_OP_FLUSH_ALL      7u

#define NPU_L1_CCR_DEFAULT        0x0001000Fu /* [VERIFIED-CODE] IB=1 | DCS=3 | ICS=3 */

#define NPU_CMX_BASE              0x2E000000u /* [VERIFIED-CODE] */
#define NPU_CMX_TILE_STRIDE       0x200000u   /* [DERIVED] */
#define NPU_DPU_REG_BASE          0x2EF30000u /* [VERIFIED-CODE] */
#define NPU_SHAVE_REG_BASE        0x2F800000u /* [VERIFIED-CODE] */

/* ============================================================================
 * 5. Boot & IPC Constants
 * ========================================================================== */

#define NPU_LOADING_ADDRESS_DONE  0x1u
#define NPU_BOOT_MSG_DATA_ADDR    0x424F4F54u /* "BOOT" */
#define NPU_IPC_CHAN_BOOT_MSG     0x3FFu
#define NPU_IPC_CHAN_ASYNC_CMD    0u          /* VPU_IPC_CHAN_ASYNC_CMD=0 */

/* ============================================================================
 * 6. Packed Structures
 * ========================================================================== */

struct npu_fw_header {
    u32 header_version;
    u32 image_format;
    u64 image_load_address;
    u32 image_size;
    u64 entry_point;
    u8  vpu_version[32];
    u32 compression_type;
    u64 firmware_version_load_address;
    u32 firmware_version_size;
    u64 boot_params_load_address;
    u32 api_version[16];
    u32 runtime_size;
    u32 shave_nn_fw_size;
    u32 preemption_buffer_1_size;
    u32 preemption_buffer_2_size;
    u32 preemption_buffer_1_max_size;
    u32 preemption_buffer_2_max_size;
    u32 preemption_reserved[4];
    u64 ro_section_start_address;
    u32 ro_section_size;
    u32 reserved;
} PACKED;

struct npu_boot_params {
    u32 magic;                      /* 0x000 */
    u32 vpu_id;                     /* 0x004 */
    u32 vpu_count;                  /* 0x008 */
    u32 reserved_0[5];              /* 0x00C */
    u32 frequency;                  /* 0x020 */
    u32 reserved_1[12];             /* 0x024 */
    u32 perf_clk_frequency;         /* 0x054 */
    u32 reserved_2[42];             /* 0x058 */
    u64 ipc_header_area_start;      /* 0x100 */
    u32 ipc_header_area_size;       /* 0x108 */
    u64 shared_region_base;         /* 0x10C */
    u32 shared_region_size;         /* 0x114 */
    u64 ipc_payload_area_start;     /* 0x118 */
    u32 ipc_payload_area_size;      /* 0x120 */
    u64 global_aliased_pio_base;    /* 0x124 */
    u32 global_aliased_pio_size;    /* 0x12C */
    u32 autoconfig;                 /* 0x130 */
    u32 cache_defaults[4];          /* 0x134 */
    u64 shave_nn_fw_base;           /* 0x144 */
    u64 save_restore_ret_address;   /* 0x14C */
    u32 reserved_4[43];             /* 0x154 */
    s32 watchdog_irq_mss;           /* 0x200 */
    s32 watchdog_irq_nce;           /* 0x204 */
    u32 host_to_vpu_irq;            /* 0x208 */
    u32 job_done_irq;               /* 0x20C */
    u32 reserved_5[60];             /* 0x210 */
    u32 host_version_id;            /* 0x300 */
    u32 si_stepping;                /* 0x304 */
    u64 device_id;                  /* 0x308 */
    u64 feature_exclusion;          /* 0x310 */
    u64 sku;                        /* 0x318 */
    u32 min_freq_pll_ratio;         /* 0x320 */
    u32 max_freq_pll_ratio;         /* 0x324 */
    u32 default_trace_level;        /* 0x328 */
    u32 boot_type;                  /* 0x32C */
    u64 punit_telemetry_sram_base;  /* 0x330 */
    u64 punit_telemetry_sram_size;  /* 0x338 */
    u32 vpu_telemetry_enable;       /* 0x340 */
    u64 crit_tracing_buff_addr;     /* 0x344 */
    u32 crit_tracing_buff_size;     /* 0x34C */
    u64 verb_tracing_buff_addr;     /* 0x350 */
    u32 verb_tracing_buff_size;     /* 0x358 */
    u64 tracing_sw_component_mask;  /* 0x35C */
    u32 trace_destination_mask;     /* 0x364 */
    u64 trace_hw_component_mask;    /* 0x368 */
    u64 tracing_buff_message_format_mask; /* 0x370 */
    u64 trace_reserved_1[2];        /* 0x378 */
    u32 pn_freq_pll_ratio;          /* 0x388 */
    u32 dvfs_mode;                  /* 0x38C */
    u64 dvfs_param;                 /* 0x390 */
    u32 reserved_6b;                /* 0x398 */
    u32 d0i3_delayed_entry;         /* 0x39C */
    u64 d0i3_residency_time_us;     /* 0x3A0 */
    u64 d0i3_entry_vpu_ts;          /* 0x3A8 */
    u64 system_time_us;             /* 0x3B0 */
    u32 reserved_7[2];              /* 0x3B8 */
    u64 device_time_delta_ticks;    /* 0x3C0 */
    u32 reserved_8[30];             /* 0x3C8 */
    u64 power_ts_active_requested;  /* 0x440 */
    u64 power_ts_active_achieved;   /* 0x448 */
    u64 power_ts_idle_requested;    /* 0x450 */
    u64 power_ts_idle_achieved;     /* 0x458 */
    u64 power_ts_standby_requested; /* 0x460 */
    u64 power_ts_standby_achieved;  /* 0x468 */
    u32 vpu_scheduling_mode;        /* 0x470 */
    u32 vpu_focus_present_timer_ms; /* 0x474 */
    u32 vpu_uses_ecc_mca_signal;    /* 0x478 */
    u32 power_profile;              /* 0x47C */
    u32 dct_active_us;              /* 0x480 */
    u32 dct_inactive_us;            /* 0x484 */
    u32 reserved_9[734];            /* 0x488 .. 0xFFF */
} PACKED;

struct npu_ipc_hdr {
    u32 data_addr;
    u32 data_size;
    u16 channel;
    u8  src_node;
    u8  dst_node;
    u8  status;
} PACKED;

#define NPU_IPC_HDR_FREE          0u
#define NPU_IPC_HDR_ALLOCATED     1u
#define NPU_IPC_SLOT_STRIDE       64u
#define NPU_IPC_TXBUF_JSM_OFF     0x40u

#define NPU_JSM_MSG_SIZE                        128u
#define NPU_JSM_MSG_QUERY_ENGINE_HB             0x1104u
#define NPU_JSM_MSG_QUERY_ENGINE_HB_DONE        0x2204u
#define NPU_JSM_MSG_SET_PRIORITY_BAND_SETUP     0x1113u
#define NPU_JSM_MSG_SET_PRIORITY_BAND_SETUP_RSP 0x2214u
#define NPU_JSM_STATUS_SUCCESS                  0x0u
#define NPU_JSM_STATUS_PARSING_ERR              0x1u
#define NPU_JSM_MSG_FREE                        0u
#define NPU_JSM_MSG_ALLOCATED                   1u

struct npu_jsm_msg {
    u64 reserved_0;
    u32 type;
    u32 status;
    u32 request_id;
    u32 result;
    u64 reserved_1;
    u8  payload[96];
} PACKED;

#define NPU_TRACING_BUFFER_CANARY 0xCAFECAFEu
#define NPU_TRACING_FORMAT_STRING 0u

struct npu_tracing_buffer_header {
    u32 host_canary_start;
    u32 read_index;
    u32 read_wrap_count;
    u32 pad0[13];
    u32 vpu_canary_start;
    u32 write_index;
    u32 wrap_count;
    u32 reserved_0;
    u32 size;
    u16 header_version;
    u16 header_size;
    u32 format;
    u32 alignment;
    char name[16];
    u32 pad1[4];
} PACKED;

static inline u32 mmio_read32(u32 addr)
{
    u32 v = *(volatile u32 *)addr;
    __asm__ volatile("" ::: "memory");
    return v;
}

static inline void mmio_write32(u32 addr, u32 val)
{
    __asm__ volatile("" ::: "memory");
    *(volatile u32 *)addr = val;
}

_Static_assert(sizeof(struct npu_fw_header) == 0xCC, "npu_fw_header size");
_Static_assert(offsetof(struct npu_fw_header, runtime_size) == 0x94, "fw hdr runtime");
_Static_assert(sizeof(struct npu_boot_params) == 0x1000, "bp size 4KB");
_Static_assert(offsetof(struct npu_boot_params, perf_clk_frequency) == 0x54, "bp perf_clk");
_Static_assert(offsetof(struct npu_boot_params, ipc_header_area_start) == 0x100, "bp ipc_hdr");
_Static_assert(offsetof(struct npu_boot_params, ipc_payload_area_start) == 0x118, "bp ipc_payload");
_Static_assert(offsetof(struct npu_boot_params, vpu_scheduling_mode) == 0x470, "bp sched");
_Static_assert(sizeof(struct npu_ipc_hdr) == 13, "ipc_hdr size");
_Static_assert(sizeof(struct npu_jsm_msg) == 128, "jsm size");
_Static_assert(sizeof(struct npu_tracing_buffer_header) == 128, "trace hdr size");

#endif /* NPU3720_HW_H */
