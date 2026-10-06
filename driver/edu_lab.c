// SPDX-License-Identifier: GPL-2.0-only
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kref.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include "include/edu_lab.h"

#define EDU_LIVE 0x04
#define EDU_FACT 0x08
#define EDU_STATUS 0x20
#define EDU_IRQ_STATUS 0x24
#define EDU_IRQ_ACK 0x64
#define EDU_BUSY 0x01
#define EDU_IRQ_ENABLE 0x80
#define EDU_FACT_IRQ 0x01
#define EDU_TIMEOUT_MS 1000
static atomic_t claimed = ATOMIC_INIT(0);
static bool fail_probe;
module_param(fail_probe, bool, 0400);
MODULE_PARM_DESC(fail_probe, "Learning hook: fail after BAR mapping to test unwind");
static bool fail_after_irq;
module_param(fail_after_irq, bool, 0400);
MODULE_PARM_DESC(fail_after_irq, "Test hook: fail after IRQ registration to test unwind");
static bool hold_completion;
module_param(hold_completion, bool, 0600);
MODULE_PARM_DESC(hold_completion, "Test hook: hold software notification after real IRQ ack");

struct edu_lab {
	void __iomem *bar;
	struct miscdevice misc;
	struct mutex op_lock; /* serializes ioctls and resource teardown */
	struct kref refs;     /* one PCI binding reference plus one per open fd */
	bool removed;        /* protected by op_lock */
	int irq;
	spinlock_t irq_lock; /* serializes ISR against request preparation */
	struct completion done;
	bool pending;        /* irq_lock: ownership survives timeout or signal */
	bool irq_seen;       /* irq_lock: only set after real FACT IRQ ack */
	bool notified;       /* irq_lock: completion delivered, including deferred hook */
	struct delayed_work notify_work;
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

static void edu_notify_work(struct work_struct *work)
{
	struct edu_lab *d = container_of(to_delayed_work(work), struct edu_lab,
					notify_work);
	unsigned long flags;

	spin_lock_irqsave(&d->irq_lock, flags);
	if (READ_ONCE(hold_completion)) {
		schedule_delayed_work(&d->notify_work, msecs_to_jiffies(10));
	} else if (d->pending && d->irq_seen && !d->notified) {
		d->notified = true;
		complete(&d->done);
	}
	spin_unlock_irqrestore(&d->irq_lock, flags);
}

static irqreturn_t edu_irq(int irq, void *cookie)
{
	struct edu_lab *d = cookie;
	unsigned long flags;
	u32 status;

	spin_lock_irqsave(&d->irq_lock, flags);
	status = readl(d->bar + EDU_IRQ_STATUS);
	if (!status) {
		spin_unlock_irqrestore(&d->irq_lock, flags);
		return IRQ_NONE; /* shared INTx, not ours */
	}
	writel(status, d->bar + EDU_IRQ_ACK);
	readl(d->bar + EDU_IRQ_STATUS); /* flush acknowledgement */
	if ((status & EDU_FACT_IRQ) && d->pending && !d->irq_seen &&
	    !(readl(d->bar + EDU_STATUS) & EDU_BUSY)) {
		d->irq_seen = true;
		if (READ_ONCE(hold_completion))
			schedule_delayed_work(&d->notify_work, msecs_to_jiffies(10));
		else {
			d->notified = true;
			complete(&d->done);
		}
	}
	spin_unlock_irqrestore(&d->irq_lock, flags);
	return IRQ_HANDLED;
}

/* Called with op_lock held. One owner, one register, no cancellation/reset. */
static int edu_factorial(struct edu_lab *d, struct edu_value *value)
{
	unsigned long flags;
	long waited;
	int ret = 0;

	if (value->input > EDU_MAX_FACTORIAL)
		return -EINVAL; /* 12! fits u32, 13! does not */
	spin_lock_irqsave(&d->irq_lock, flags);
	if (readl(d->bar + EDU_STATUS) & EDU_BUSY) {
		ret = -EBUSY;
		goto unlock;
	}
	/* irq_seen means the old IRQ has been acked. Holding irq_lock also
	 * excludes the old ISR's complete() from racing reinitialization. */
	d->pending = true;
	d->irq_seen = false;
	d->notified = false;
	reinit_completion(&d->done);
	writel(EDU_IRQ_ENABLE, d->bar + EDU_STATUS);
	writel(value->input, d->bar + EDU_FACT);
	readl(d->bar + EDU_STATUS); /* flush start */
unlock:
	spin_unlock_irqrestore(&d->irq_lock, flags);
	if (ret)
		return ret;
	waited = wait_for_completion_interruptible_timeout(&d->done,
					msecs_to_jiffies(EDU_TIMEOUT_MS));
	if (waited <= 0)
		return waited ? (int)waited : -ETIMEDOUT;
	spin_lock_irqsave(&d->irq_lock, flags);
	if (!d->irq_seen || !d->notified ||
	    (readl(d->bar + EDU_STATUS) & EDU_BUSY))
		ret = -EIO;
	else {
		value->result = readl(d->bar + EDU_FACT);
		d->pending = false;
	}
	spin_unlock_irqrestore(&d->irq_lock, flags);
	return ret;
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
	if (cmd != EDU_IOC_LIVE && cmd != EDU_IOC_FACTORIAL) {
		ret = -ENOTTY; goto out;
	}
	if (copy_from_user(&value, ptr, sizeof(value))) {
		ret = -EFAULT; goto out;
	}
	if (cmd == EDU_IOC_LIVE) {
		writel(value.input, d->bar + EDU_LIVE);
		value.result = readl(d->bar + EDU_LIVE); /* flush posted write */
	} else {
		ret = edu_factorial(d, &value);
		if (ret) goto out;
	}
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
	spin_lock_init(&d->irq_lock);
	init_completion(&d->done);
	INIT_DELAYED_WORK(&d->notify_work, edu_notify_work);
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
	/* INTx is deliberately selected for the shared-IRQ learning path. */
	pci_intx(pdev, 0);
	writel(0, d->bar + EDU_STATUS);
	writel(~0U, d->bar + EDU_IRQ_ACK);
	readl(d->bar + EDU_IRQ_STATUS);
	if (readl(d->bar + EDU_STATUS) & EDU_BUSY) {
		ret = -EBUSY; goto unmap;
	}
	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_INTX);
	if (ret < 0) goto unmap;
	d->irq = pci_irq_vector(pdev, 0);
	ret = request_irq(d->irq, edu_irq, IRQF_SHARED, "edu_lab", d);
	if (ret) goto vectors;
	pci_intx(pdev, 1);
	if (fail_after_irq) { ret = -EIO; goto irq; }
	d->misc.minor = MISC_DYNAMIC_MINOR;
	d->misc.name = "edu_lab";
	d->misc.fops = &edu_fops;
	d->misc.parent = &pdev->dev;
	d->misc.mode = 0600;
	ret = misc_register(&d->misc);
	if (ret) goto irq;
	pci_set_drvdata(pdev, d);
	dev_info(&pdev->dev, "EDU ready, id=%08x\n", readl(d->bar));
	return 0;
irq:
	pci_intx(pdev, 0);
	free_irq(d->irq, d);
	cancel_delayed_work_sync(&d->notify_work);
vectors:
	pci_free_irq_vectors(pdev);
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
	/* Mask at PCI level too: no reset/cancel exists, and a computation
	 * thread could already have decided to raise a late interrupt. */
	writel(0, d->bar + EDU_STATUS);
	readl(d->bar + EDU_STATUS);
	pci_intx(pdev, 0);
	free_irq(d->irq, d); /* waits for ISR before MMIO/object release */
	cancel_delayed_work_sync(&d->notify_work); /* ISR cannot enqueue now */
	pci_free_irq_vectors(pdev);
	writel(~0U, d->bar + EDU_IRQ_ACK);
	readl(d->bar + EDU_IRQ_STATUS);
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
