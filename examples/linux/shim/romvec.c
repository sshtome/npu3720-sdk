/* SPDX-License-Identifier: MIT */
#include "romvec.h"

static int str_eq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static void *shim_memset(void *s, int c, u32 n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--)
        *p++ = (unsigned char)c;
    return s;
}

/*
 * Implementation of minimal OpenBoot/PROM services for Linux SPARC.
 * Routes earlycon output directly to fw_log ring buffer in shared DDR.
 */

static struct linux_romvec s_romvec;
static struct linux_nodeops s_nodeops;

/* Memory description: 48 MB DDR available at 0x85000000 */
static struct linux_mlist_v0 s_mem_avail = {
    .theres_more = NULL,
    .start_adr = 0x85000000u,
    .num_bytes = 48u * 1024u * 1024u,
};

static struct linux_mlist_v0 *s_avail_list = &s_mem_avail;
static struct linux_mlist_v0 *s_totphys_list = &s_mem_avail;
static struct linux_mlist_v0 *s_prommap_list = NULL;

static struct linux_arguments_v0 s_bootargs_v0;
static struct linux_arguments_v0 *s_bootargs_v0_ptr = &s_bootargs_v0;

/* Node handles: root node = 1 */
#define PROM_NODE_ROOT 1

static s32 shim_no_nextnode(s32 node)
{
    (void)node;
    return 0; /* Only 1 node (root) exists */
}

static s32 shim_no_child(s32 node)
{
    (void)node;
    return 0;
}

static s32 shim_no_proplen(s32 node, const char *name)
{
    (void)node;
    if (!name)
        return -1;
    if (str_eq(name, "compatible"))
        return 5;
    if (str_eq(name, "device_type"))
        return 4;
    if (str_eq(name, "mmu-nctx"))
        return 4;
    return -1;
}

static s32 shim_no_getprop(s32 node, const char *name, char *val)
{
    (void)node;
    if (!name || !val)
        return -1;

    if (str_eq(name, "compatible")) {
        val[0] = 'l';
        val[1] = 'e';
        val[2] = 'o';
        val[3] = 'n';
        val[4] = '\0';
        return 5;
    }

    if (str_eq(name, "device_type")) {
        val[0] = 'c';
        val[1] = 'p';
        val[2] = 'u';
        val[3] = '\0';
        return 4;
    }

    if (str_eq(name, "mmu-nctx")) {
        u32 nctx = 256;
        *(u32 *)val = nctx;
        return 4;
    }

    return -1;
}

static s32 shim_no_setprop(s32 node, const char *name, char *val, s32 len)
{
    (void)node; (void)name; (void)val; (void)len;
    return -1;
}

static char *shim_no_nextprop(s32 node, char *name)
{
    (void)node; (void)name;
    return "";
}

/* Console I/O functions */
static s32 shim_pv_getchar(void)
{
    return -1;
}

static void shim_pv_putchar(s32 ch)
{
    npu_log_putc((char)ch);
}

static s32 shim_pv_nbgetchar(void)
{
    return -1;
}

static s32 shim_pv_nbputchar(s32 ch)
{
    npu_log_putc((char)ch);
    return 1;
}

static void shim_pv_putstr(char *str, s32 len)
{
    if (!str || len <= 0)
        return;
    for (s32 i = 0; i < len; i++)
        npu_log_putc(str[i]);
}

static void shim_pv_reboot(char *bootstr)
{
    (void)bootstr;
    npu_log_puts("[SHIM] Linux requested reboot, halting CPU.\n");
    for (;;) {
        __asm__ volatile("nop");
    }
}

static void shim_pv_printf(const char *fmt, ...)
{
    /* Minimal fallback */
    npu_log_puts("[SHIM] ");
    npu_log_puts(fmt ? fmt : "(null)");
}

static void shim_pv_abort(void)
{
    npu_log_puts("[SHIM] Linux called pv_abort!\n");
    for (;;) {
        __asm__ volatile("nop");
    }
}

static void shim_pv_halt(void)
{
    npu_log_puts("[SHIM] Linux called pv_halt.\n");
    for (;;) {
        __asm__ volatile("nop");
    }
}

struct linux_romvec *shim_romvec_init(const char *cmdline)
{
    shim_memset(&s_romvec, 0, sizeof(s_romvec));
    shim_memset(&s_nodeops, 0, sizeof(s_nodeops));
    shim_memset(&s_bootargs_v0, 0, sizeof(s_bootargs_v0));

    /* Node operations */
    s_nodeops.no_nextnode = shim_no_nextnode;
    s_nodeops.no_child = shim_no_child;
    s_nodeops.no_proplen = shim_no_proplen;
    s_nodeops.no_getprop = shim_no_getprop;
    s_nodeops.no_setprop = shim_no_setprop;
    s_nodeops.no_nextprop = shim_no_nextprop;

    /* Boot arguments */
    if (cmdline) {
        int i = 0;
        while (cmdline[i] && i < 99) {
            s_bootargs_v0.args[i] = cmdline[i];
            i++;
        }
        s_bootargs_v0.args[i] = '\0';
    } else {
        s_bootargs_v0.args[0] = '\0';
    }
    s_bootargs_v0.argv[0] = s_bootargs_v0.args;

    /* Top level romvec */
    s_romvec.pv_magic_cookie = 0x10010407u;
    s_romvec.pv_romvers = 0; /* Version 0 */
    s_romvec.pv_plugin_revision = 1;
    s_romvec.pv_printrev = 0x00020000u;

    s_romvec.pv_v0mem.v0_available = &s_avail_list;
    s_romvec.pv_v0mem.v0_totphys   = &s_totphys_list;
    s_romvec.pv_v0mem.v0_prommap   = &s_prommap_list;

    s_romvec.pv_nodeops = &s_nodeops;

    s_romvec.pv_getchar = shim_pv_getchar;
    s_romvec.pv_putchar = shim_pv_putchar;
    s_romvec.pv_nbgetchar = shim_pv_nbgetchar;
    s_romvec.pv_nbputchar = shim_pv_nbputchar;
    s_romvec.pv_putstr = shim_pv_putstr;
    s_romvec.pv_reboot = shim_pv_reboot;
    s_romvec.pv_printf = shim_pv_printf;
    s_romvec.pv_abort  = shim_pv_abort;
    s_romvec.pv_halt   = shim_pv_halt;

    s_romvec.pv_v0bootargs = &s_bootargs_v0_ptr;

    return &s_romvec;
}

