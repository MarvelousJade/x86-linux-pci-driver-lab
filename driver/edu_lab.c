// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>

static atomic_t claimed = ATOMIC_INIT(0);
static bool fail_probe;
module_param(fail_probe, bool, 0400);
MODULE_PARM_DESC(fail_probe, "Learning hook: fail after BAR mapping to test unwind");
struct edu_lab { void __iomem *bar; };

static int edu_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	struct edu_lab *d;
	int ret;

	if (atomic_cmpxchg(&claimed, 0, 1))
		return -EBUSY;
	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d) { ret = -ENOMEM; goto claim; }
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
	pci_set_drvdata(pdev, d);
	dev_info(&pdev->dev, "EDU BAR0 mapped, id=%08x\n", readl(d->bar));
	return 0;
unmap:
	pci_iounmap(pdev, d->bar);
region:
	pci_release_region(pdev, 0);
disable:
	pci_disable_device(pdev);
object:
	kfree(d);
claim:
	atomic_set(&claimed, 0);
	return ret;
}

static void edu_remove(struct pci_dev *pdev)
{
	struct edu_lab *d = pci_get_drvdata(pdev);
	pci_iounmap(pdev, d->bar);
	pci_disable_device(pdev);
	pci_release_region(pdev, 0);
	kfree(d);
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
