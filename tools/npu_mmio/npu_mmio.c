// npu_mmio — debugfs BAR0 peek/poke module for Intel NPU 3720 (8086:ad1d).
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>

#define NPU_VENDOR 0x8086
#define NPU_DEVICE 0xad1d
#define BAR0_SIZE  (128 * 1024 * 1024)

static struct pci_dev *pdev;
static void __iomem *bar0;
static struct dentry *dir;
static u32 last_peek;

static ssize_t peek_write(struct file *f, const char __user *buf,
			  size_t len, loff_t *off)
{
	char kb[32];
	u64 v;
	if (len >= sizeof(kb))
		return -EINVAL;
	if (copy_from_user(kb, buf, len))
		return -EFAULT;
	kb[len] = 0;
	if (kstrtou64(kb, 16, &v) || v >= BAR0_SIZE - 4)
		return -EINVAL;
	last_peek = ioread32(bar0 + v);
	return len;
}

static int peek_show(struct seq_file *s, void *p)
{
	seq_printf(s, "0x%08x\n", last_peek);
	return 0;
}

static int peek_open(struct inode *i, struct file *f)
{
	return single_open(f, peek_show, NULL);
}

static const struct file_operations peek_fops = {
	.owner = THIS_MODULE,
	.open = peek_open,
	.read = seq_read,
	.write = peek_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t poke_write(struct file *f, const char __user *buf,
			  size_t len, loff_t *off)
{
	char kb[64];
	u64 o, v;
	char *p;
	if (len >= sizeof(kb))
		return -EINVAL;
	if (copy_from_user(kb, buf, len))
		return -EFAULT;
	kb[len] = 0;
	p = strchr(kb, ' ');
	if (!p)
		return -EINVAL;
	*p++ = 0;
	if (kstrtou64(kb, 16, &o) || kstrtou64(p, 16, &v) || o >= BAR0_SIZE - 4)
		return -EINVAL;
	iowrite32((u32)v, bar0 + o);
	return len;
}

static const struct file_operations poke_fops = {
	.owner = THIS_MODULE,
	.write = poke_write,
};

static int __init npu_mmio_init(void)
{
	int ret;

	pdev = pci_get_device(NPU_VENDOR, NPU_DEVICE, NULL);
	if (!pdev)
		return -ENODEV;

	bar0 = pci_ioremap_bar(pdev, 0);
	if (!bar0) {
		ret = -ENOMEM;
		goto err_put;
	}

	pm_runtime_get_sync(&pdev->dev);
	dir = debugfs_create_dir("npu_mmio", NULL);
	debugfs_create_file("peek", 0644, dir, NULL, &peek_fops);
	debugfs_create_file("poke", 0200, dir, NULL, &poke_fops);
	return 0;

err_put:
	pci_dev_put(pdev);
	return ret;
}

static void __exit npu_mmio_exit(void)
{
	debugfs_remove_recursive(dir);
	pm_runtime_put(&pdev->dev);
	iounmap(bar0);
	pci_dev_put(pdev);
}

module_init(npu_mmio_init);
module_exit(npu_mmio_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Debugfs BAR0 peek/poke access for Intel NPU 3720");
