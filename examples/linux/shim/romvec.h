/* SPDX-License-Identifier: MIT */
#ifndef NPU_SHIM_ROMVEC_H
#define NPU_SHIM_ROMVEC_H

#include "npu3720_sdk.h"

/*
 * Minimal Linux romvec (SPARC OpenBoot/PROM compatibility) for NPU 3720 shim.
 */

struct linux_mlist_v0 {
    struct linux_mlist_v0 *theres_more;
    u32 start_adr;
    u32 num_bytes;
};

struct linux_mem_v0 {
    struct linux_mlist_v0 **v0_totphys;
    struct linux_mlist_v0 **v0_prommap;
    struct linux_mlist_v0 **v0_available;
};

struct linux_nodeops {
    s32 (*no_nextnode)(s32 node);
    s32 (*no_child)(s32 node);
    s32 (*no_proplen)(s32 node, const char *name);
    s32 (*no_getprop)(s32 node, const char *name, char *val);
    s32 (*no_setprop)(s32 node, const char *name, char *val, s32 len);
    char *(*no_nextprop)(s32 node, char *name);
};

struct linux_arguments_v0 {
    char *argv[8];
    char args[100];
    char boot_dev[2];
    s32 boot_dev_ctrl;
    s32 boot_dev_unit;
    s32 dev_partition;
    char *kernel_file_name;
    void *aieee1;
};

struct linux_dev_v0_funcs {
    s32 (*v0_devopen)(char *device_str);
    s32 (*v0_devclose)(s32 dev_desc);
    s32 (*v0_rdblkdev)(s32 dev_desc, s32 num_blks, s32 blk_st, char *buf);
    s32 (*v0_wrblkdev)(s32 dev_desc, s32 num_blks, s32 blk_st, char *buf);
    s32 (*v0_wrnetdev)(s32 dev_desc, s32 num_bytes, char *buf);
    s32 (*v0_rdnetdev)(s32 dev_desc, s32 num_bytes, char *buf);
    s32 (*v0_rdchardev)(s32 dev_desc, s32 num_bytes, s32 dummy, char *buf);
    s32 (*v0_wrchardev)(s32 dev_desc, s32 num_bytes, s32 dummy, char *buf);
    s32 (*v0_seekdev)(s32 dev_desc, long logical_offst, s32 from);
};

struct linux_bootargs_v2 {
    char **bootpath;
    char **bootargs;
    s32 *fd_stdin;
    s32 *fd_stdout;
};

struct linux_dev_v2_funcs {
    s32 (*v2_inst2pkg)(s32 d);
    char *(*v2_dumb_mem_alloc)(char *va, u32 sz);
    void (*v2_dumb_mem_free)(char *va, u32 sz);
    char *(*v2_dumb_mmap)(char *virta, s32 which_io, u32 paddr, u32 sz);
    void (*v2_dumb_munmap)(char *virta, u32 size);
    s32 (*v2_dev_open)(char *devpath);
    void (*v2_dev_close)(s32 d);
    s32 (*v2_dev_read)(s32 d, char *buf, s32 nbytes);
    s32 (*v2_dev_write)(s32 d, const char *buf, s32 nbytes);
    s32 (*v2_dev_seek)(s32 d, s32 hi, s32 lo);
    void (*v2_wheee2)(void);
    void (*v2_wheee3)(void);
};

struct linux_romvec {
    u32 pv_magic_cookie;
    u32 pv_romvers;
    u32 pv_plugin_revision;
    u32 pv_printrev;
    struct linux_mem_v0 pv_v0mem;
    struct linux_nodeops *pv_nodeops;
    char **pv_bootstr;
    struct linux_dev_v0_funcs pv_v0devops;
    char *pv_stdin;
    char *pv_stdout;
    s32 (*pv_getchar)(void);
    void (*pv_putchar)(s32 ch);
    s32 (*pv_nbgetchar)(void);
    s32 (*pv_nbputchar)(s32 ch);
    void (*pv_putstr)(char *str, s32 len);
    void (*pv_reboot)(char *bootstr);
    void (*pv_printf)(const char *fmt, ...);
    void (*pv_abort)(void);
    volatile s32 *pv_ticks;
    void (*pv_halt)(void);
    void (**pv_synchook)(void);
    union {
        void (*v0_eval)(s32 len, char *str);
        void (*v2_eval)(char *str);
    } pv_fortheval;
    struct linux_arguments_v0 **pv_v0bootargs;
    u32 (*pv_enaddr)(s32 d, char *enaddr);
    struct linux_bootargs_v2 pv_v2bootargs;
    struct linux_dev_v2_funcs pv_v2devops;
    s32 filler[15];
};

/* Public API */
struct linux_romvec *shim_romvec_init(const char *cmdline);

#endif /* NPU_SHIM_ROMVEC_H */
