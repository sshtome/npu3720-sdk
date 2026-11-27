/*
 * ipc_jsm.c — D2H/H2D IPC ring slots, BOOT handshake, and JSM responder.
 */
#include "npu3720_sdk.h"

static u32 hdr_area;
static u32 payload_area;
static u32 hdr_slot;
static u32 payload_slot;
static u32 jsm_msg_count;
static u32 jsm_last_msg_type;

void npu_ipc_init(void)
{
    const struct npu_boot_params *bp =
        (const struct npu_boot_params *)NPU_BOOT_PARAMS_ADDR;
    hdr_area = (u32)bp->ipc_header_area_start;
    payload_area = (u32)bp->ipc_payload_area_start;
    hdr_slot = 0;
    payload_slot = 0;
    jsm_msg_count = 0;
    jsm_last_msg_type = 0;
}

void npu_ipc_send(u32 data_addr, u32 data_size, u16 channel)
{
    u32 hslot = hdr_area + NPU_IPC_SLOT_STRIDE * (hdr_slot & 0x7Fu);
    volatile struct npu_ipc_hdr *h = (volatile struct npu_ipc_hdr *)hslot;

    h->data_addr = data_addr;
    h->data_size = data_size;
    h->channel = channel;
    h->src_node = 1;
    h->dst_node = 0;
    h->status = NPU_IPC_HDR_ALLOCATED;

    if (npu_l1_is_flush_enabled())
        npu_l1_dcache_flush();
    if (!npu_l2c_is_mtrr_active()) {
        if (data_size > 0 && data_addr >= 0x80000000u)
            npu_l2c_clean_range(data_addr, data_size);
        npu_l2c_clean_range(hslot, NPU_IPC_SLOT_STRIDE);
    } else if (data_size > 0 && data_addr >= 0x80000000u &&
               (data_addr < NPU_SHARED_REGION_BASE ||
                data_addr >= NPU_SHARED_REGION_BASE + NPU_SHARED_UNCACHED_SIZE)) {
        npu_l2c_clean_range(data_addr, data_size);
    }

    mmio_write32(NPU_D2H_FIFO_WRITE, (hslot & ~0x3Fu) | NPU_D2H_DESC_SRC_ID_CORE0);
    hdr_slot++;
}

void npu_ipc_send_boot(void)
{
    npu_ipc_send(NPU_BOOT_MSG_DATA_ADDR, 0, NPU_IPC_CHAN_BOOT_MSG);
}

static void jsm_respond(const volatile struct npu_jsm_msg *req, u32 resp_type)
{
    u32 pslot = payload_area + NPU_JSM_MSG_SIZE * (payload_slot & 0x3Fu);
    volatile struct npu_jsm_msg *m = (volatile struct npu_jsm_msg *)pslot;
    const volatile u32 *src_p = (const volatile u32 *)req->payload;
    volatile u32 *dst_p = (volatile u32 *)m->payload;
    u32 i;

    m->reserved_0 = 0;
    m->type = resp_type;
    m->status = NPU_JSM_MSG_ALLOCATED;
    m->request_id = req->request_id;
    m->result = NPU_JSM_STATUS_SUCCESS;
    m->reserved_1 = 0;
    for (i = 0; i < sizeof(m->payload) / 4u; i++)
        dst_p[i] = src_p[i];

    npu_ipc_send(pslot, NPU_JSM_MSG_SIZE, NPU_IPC_CHAN_ASYNC_CMD);
    payload_slot++;
}

void npu_jsm_poll(void)
{
    u32 psr = npu_irq_save();
    u32 stat = mmio_read32(NPU_H2D_FIFO_STAT);
    u32 fill = (stat >> 16) & 0xFFu;
    u32 addr;
    volatile struct npu_ipc_hdr *hdr;
    volatile struct npu_jsm_msg *req;

    if (fill == 0) {
        npu_irq_restore(psr);
        return;
    }

    addr = mmio_read32(NPU_H2D_FIFO_ATM);

    if ((addr & 0xFF000000u) == NPU_CONSOLE_TAG) {
        npu_console_rx_word(addr);
        mmio_write32(NPU_H2D_FIFO_ACK, NPU_H2D_POP_ACK_VAL);
        npu_irq_restore(psr);
        return;
    }

    hdr = (volatile struct npu_ipc_hdr *)addr;
    req = (volatile struct npu_jsm_msg *)(addr + NPU_IPC_TXBUF_JSM_OFF);

    jsm_msg_count++;
    jsm_last_msg_type = req->type;

    npu_log_puts("jsm:chan=");
    npu_log_hex(hdr->channel);
    npu_log_puts(" type=");
    npu_log_hex(req->type);
    npu_log_puts(" rid=");
    npu_log_hex(req->request_id);
    npu_log_puts("\n");

    switch (req->type) {
    case NPU_JSM_MSG_SET_PRIORITY_BAND_SETUP:
        jsm_respond(req, NPU_JSM_MSG_SET_PRIORITY_BAND_SETUP_RSP);
        break;
    case NPU_JSM_MSG_QUERY_ENGINE_HB:
        jsm_respond(req, NPU_JSM_MSG_QUERY_ENGINE_HB_DONE);
        break;
    default:
        npu_log_puts("jsm:unknown type\n");
        break;
    }

    mmio_write32(NPU_H2D_FIFO_ACK, NPU_H2D_POP_ACK_VAL);
    npu_irq_restore(psr);
}

static void on_h2d_fifo_irq(u32 irq, void *ctx)
{
    (void)irq;
    (void)ctx;
    while (((mmio_read32(NPU_H2D_FIFO_STAT) >> 16) & 0xFFu) != 0u)
        npu_jsm_poll();
}

void npu_ipc_enable_irq(void)
{
    npu_irq_register(NPU_IRQ_H2D_FIFO, on_h2d_fifo_irq, NULL);
    npu_irq_enable(NPU_IRQ_H2D_FIFO, 7u, 1);
}

u32 npu_jsm_count(void)
{
    return jsm_msg_count;
}

u32 npu_jsm_last_type(void)
{
    return jsm_last_msg_type;
}
