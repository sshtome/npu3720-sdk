/*
 * log.c — coherent fw_log tracing buffer (0x90000000).
 */
#include "npu3720_sdk.h"

#define LOG_BUF_SIZE 0x80000u

static volatile struct npu_tracing_buffer_header *const log_hdr =
    (volatile struct npu_tracing_buffer_header *)NPU_LOG_CRIT_BUF;

static u32 log_pos;
static u32 log_flushed_pos;

void npu_log_init(const char *name)
{
    u32 i;
    for (i = 0; i < sizeof(struct npu_tracing_buffer_header) / 4u; i++)
        mmio_write32(NPU_LOG_CRIT_BUF + i * 4u, 0);

    log_hdr->vpu_canary_start = NPU_TRACING_BUFFER_CANARY;
    log_hdr->header_version = 1;
    log_hdr->header_size = 128;
    log_hdr->format = NPU_TRACING_FORMAT_STRING;
    log_hdr->size = LOG_BUF_SIZE;
    log_hdr->alignment = 64;

    if (name) {
        for (i = 0; i < 15u && name[i]; i++)
            log_hdr->name[i] = name[i];
        log_hdr->name[i] = '\0';
    }

    log_pos = 128u;
    log_flushed_pos = 128u;
    log_hdr->write_index = log_pos;
    npu_log_flush();
}

void npu_log_flush(void)
{
    int mtrr = npu_l2c_is_mtrr_active();

    if (npu_l1_is_flush_enabled())
        npu_l1_dcache_flush();
    if (!mtrr)
        npu_l2c_clean_range(NPU_LOG_CRIT_BUF + 0x40u, 0x40u);
    if (log_pos > log_flushed_pos) {
        if (!mtrr)
            npu_l2c_clean_range(NPU_LOG_CRIT_BUF + log_flushed_pos, log_pos - log_flushed_pos);
        log_flushed_pos = log_pos;
    }
}

void npu_log_putc(char c)
{
    if (log_pos >= LOG_BUF_SIZE) {
        log_pos = 128u;
        log_flushed_pos = 128u;
        log_hdr->wrap_count++;
    }
    *(volatile u8 *)(NPU_LOG_CRIT_BUF + log_pos) = (u8)c;
    log_pos++;
    log_hdr->write_index = log_pos;
}

void npu_log_puts(const char *s)
{
    if (!s)
        return;
    while (*s)
        npu_log_putc(*s++);
    npu_log_flush();
}

void npu_log_hex(u32 v)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[9];
    int i;

    for (i = 7; i >= 0; i--) {
        buf[i] = hex[v & 0xFu];
        v >>= 4;
    }
    buf[8] = '\0';
    npu_log_puts(buf);
}

void npu_log_dec(u32 v)
{
    char buf[12];
    int i = 0;

    if (v == 0) {
        npu_log_puts("0");
        return;
    }
    while (v > 0 && i < 11) {
        buf[i++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (i > 0)
        npu_log_putc(buf[--i]);
    npu_log_flush();
}
