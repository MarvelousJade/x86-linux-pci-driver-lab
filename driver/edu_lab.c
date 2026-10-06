// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include "include/edu_lab.h"

#define EDU_LIVE 0x04
static atomic_t claimed = ATOMIC_INIT(0);
static bool fail_probe;
module_param(fail_probe, bool, 0400);
MODULE_PARM_DESC(fail_probe, "Learning hook: fail after BAR mapping to test unwind");

struct edu_lab {
	void __iomem *bar;
	struct miscdevice misc;
	struct mutex op_lock; /* serializes ioctls and resource teardown */
	struct kref refs;     /* one PCI binding reference plus one per open fd */
	bool removed;        /* protected by op_lock */
};

static void edu_free(struct kref *ref)
{
	kfree(container_of(ref, struct edu_lab, refs));
}

static int edu_open(struct inode *inode, struct file *file)
{
	struct edu_lab *d = container_of(file->private_data, struct edu_lab, misc);
	int ret = 0;

	/* misc core serializes this callback against misc_deregister(). The
	 * binding reference is still held while we acquire the fd reference. */
	mutex_lock(&d->op_lock);
	if (d->removed)
		ret = -ENODEV;
	else {
		kref_get(&d->refs);
		file->private_data = d;
	}
	mutex_unlock(&d->op_lock);
	return ret;
}

static int edu_release(struct inode *inode, struct file *file)
{
	struct edu_lab *d = file->private_data;
	kref_put(&d->refs, edu_free);
	return 0;
}

static long edu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct edu_lab *d = file->private_data;
	struct edu_value value;
	void __user *ptr = (void __user *)arg;
	int ret;

	if (mutex_lock_interruptible(&d->op_lock))
		return -ERESTARTSYS;
	if (d->removed) { ret = -ENODEV; goto out; }
	if (cmd != EDU_IOC_LIVE) { ret = -ENOTTY; goto out; }
	if (copy_from_user(&value, ptr, sizeof(value))) {
		ret = -EFAULT; goto out;
	}
	writel(value.input, d->bar + EDU_LIVE);
	value.result = readl(d->bar + EDU_LIVE); /* flush posted write */
	ret = copy_to_user(ptr, &value, sizeof(value)) ? -EFAULT : 0;
out:
	mutex_unlock(&d->op_lock);
	return ret;
}

static const struct file_operations edu_fops = {
	.owner = THIS_MODULE, .open = edu_open, .release = edu_release,
	.unlocked_ioctl = edu_ioctl, /* NULL llseek: ESPIPE on current kernels */
};

static int edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu_lab *d;
	int ret;

	if (atomic_cmpxchg(&claimed, 0, 1))
		return -EBUSY;
	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d) { ret = -ENOMEM; goto claim; }
	mutex_init(&d->op_lock);
	kref_init(&d->refs);
	ret = pci_enable_device_mem(pdev);
	if (ret) goto object;
	if (!(pci_resource_flags(pdev, 0) & IORESOURCE_MEM) ||
	    pci_resource_len(pdev, 0) < 0x68) {
		ret = -ENODEV; goto disable;
	}
	ret = pci_request_region(pdev, 0, "edu_lab");
	if (ret) goto disable;
	d->bar = pci_iomap(pdev, 0, 0);
	if (!d->bar) { ret = -ENOMEM; goto region; }
	if (fail_probe) { ret = -EIO; goto unmap; }
	d->misc.minor = MISC_DYNAMIC_MINOR;
	d->misc.name = "edu_lab";
	d->misc.fops = &edu_fops;
	d->misc.parent = &pdev->dev;
	d->misc.mode = 0600;
	ret = misc_register(&d->misc);
	if (ret) goto unmap;
	pci_set_drvdata(pdev, d);
	dev_info(&pdev->dev, "EDU ready, id=%08x\n", readl(d->bar));
	return 0;
unmap:
	pci_iounmap(pdev, d->bar);
region:
	pci_release_region(pdev, 0);
disable:
	pci_disable_device(pdev);
object:
	kref_put(&d->refs, edu_free);
claim:
	atomic_set(&claimed, 0);
	return ret;
}

static void edu_remove(struct pci_dev *pdev)
{
	struct edu_lab *d = pci_get_drvdata(pdev);

	misc_deregister(&d->misc); /* no new opens before dropping binding ref */
	mutex_lock(&d->op_lock);
	d->removed = true;
	pci_iounmap(pdev, d->bar);
	pci_disable_device(pdev);
	pci_release_region(pdev, 0);
	mutex_unlock(&d->op_lock);
	pci_set_drvdata(pdev, NULL);
	kref_put(&d->refs, edu_free);
	atomic_set(&claimed, 0);
	dev_info(&pdev->dev, "EDU released\n");
}
static const struct pci_device_id edu_ids[] = {
	{ PCI_DEVICE(0x1234, 0x11e8) }, { }
};
MODULE_DEVICE_TABLE(pci, edu_ids);
static struct pci_driver edu_driver = {
	.name = "edu_lab", .id_table = edu_ids,
	.probe = edu_probe, .remove = edu_remove,
};
module_pci_driver(edu_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Small QEMU EDU PCI driver lab (no DMA)");
