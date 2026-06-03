#include "mybell.h"
#include <linux/dev_printk.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>

static irqreturn_t mb_irq_handler(int irq, void *dev_id)
{
	struct mybell_dev *d = dev_id;
	u32 irqst;
	irqst = mb_get_irq_status(d);
	if (!(irqst & MB_IRQ_STATUS_PENDING))
		return IRQ_NONE;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t mb_irq_thread(int irq, void *dev_id)
{
	struct mybell_dev *d = dev_id;
	u32 status;
	u32 event_count;
	u32 irq_status_before;
	u32 irq_status_after;

	status = mb_get_status(d);
	event_count = mb_get_event_count(d);
	irq_status_before = mb_get_irq_status(d);

	d->irq_count++;
	dev_info(
		&d->pdev->dev,
		"irq handled: irq_count=%u event_count=%u status=0x%x active=%u irq_status_before=0x%x\n",
		d->irq_count, event_count, status,
		!!(status & MB_STATUS_EVENT_ACTIVE), irq_status_before);

	dev_info(&d->pdev->dev, "about to ack irq\n");
	mb_ack_irq(d);
	dev_info(&d->pdev->dev, "ack irq done\n");

	irq_status_after = mb_get_irq_status(d);
	dev_info(&d->pdev->dev, "irq_status_after=0x%x\n", irq_status_after);

	return IRQ_HANDLED;
}

static int mybell_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *base;
	int irq;
	int ret;
	struct mybell_dev *mydev;

	// get platform resource
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == res) {
		dev_err(&pdev->dev, "no IORESOURCE_MEM found\n");
		return -ENODEV;
	}

	// allocate mydev
	mydev = devm_kzalloc(&pdev->dev, sizeof(struct mybell_dev), GFP_KERNEL);
	if (NULL == mydev) {
		dev_err(&pdev->dev, "mydev probe kzalloc error");
		return -ENOMEM;
	}
	mydev->pdev = pdev;

	// physical addr to virtual addr
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev, "mydev probe ioremap error");
		return PTR_ERR(base);
	}
	mydev->base = base;

	// get irq num
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "mydev probe get irq error");
		return irq;
	}
	mydev->irq = irq;

	/* Clear any stale pending state before enabling interrupt */
	mb_ack_irq(mydev);
	ret = devm_request_threaded_irq(&mydev->pdev->dev, mydev->irq,
					mb_irq_handler, mb_irq_thread,
					IRQF_ONESHOT, NULL, mydev); //IRQF_ONESHOT is important, or there will be interrupt storm, you can see the storm by 'cat /proc/interrupts'
	if (ret) {
		dev_err(&pdev->dev, "request irq handler error");
		return ret;
	}
	mb_enable_irq(mydev);

	platform_set_drvdata(pdev, mydev);

	dev_info(&pdev->dev,
		 "probe success: irq=%d status=0x%x event_count=%u\n",
		 mydev->irq, mb_get_status(mydev), mb_get_event_count(mydev));

	return 0;
}

static void mybell_remove(struct platform_device *pdev)
{
	struct mybell_dev *mydev = platform_get_drvdata(pdev);
	mb_disable_irq(mydev);
	dev_info(&pdev->dev, "removed, total irq_count=%u\n", mydev->irq_count);
}

static const struct of_device_id mybell_dfs_match[] = {
	{ .compatible = "zxy,fakebell" },
	{},
};
MODULE_DEVICE_TABLE(of, mybell_dfs_match);

static struct platform_driver mybell_driver = {
    .probe = mybell_probe,
    .remove = mybell_remove,
    .driver =
        {
            .name = "mybell_driver",
            .of_match_table = mybell_dfs_match,
        },
};

static int __init mybell_init(void)
{
	pr_info("mybell: register");
	int ret;
	ret = platform_driver_register(&mybell_driver);
	if (ret)
		pr_err("mybell: register driver error");
	return ret;
}

static void __exit mybell_exit(void)
{
	platform_driver_unregister(&mybell_driver);
	pr_info("mybell: unregister");
	return;
}

module_init(mybell_init);
module_exit(mybell_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxy");
MODULE_DESCRIPTION("Bell monitor");
