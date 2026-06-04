#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/fs.h>
#include "mybell.h"

#define MAX_DEVICE 16

static struct class *mycls = NULL;
static dev_t devnum;

struct myalarmdev {
	struct platform_device *pdev;
	struct cdev almcdev;
	struct device *pcdev;
	wait_queue_head_t wq;
	struct mutex lock;
        struct mybell_dev mb; 
	int alarm;
};

static irqreturn_t alarm_irq_handler(int irq, void *dev_id)
{
	struct myalarmdev *d = dev_id;
	u32 irqst;
	irqst = mb_get_irq_status(&d->mb);
	if (!(irqst & MB_IRQ_STATUS_PENDING))
		return IRQ_NONE;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t alarm_irq_thread(int irq, void *dev_id)
{
	struct myalarmdev *d = dev_id;
	u32 status;
	u32 event_count;
	u32 irq_status_before;
	u32 irq_status_after;

	status = mb_get_status(&d->mb);
	event_count = mb_get_event_count(&d->mb);
	irq_status_before = mb_get_irq_status(&d->mb);

	dev_info(
		&d->pdev->dev,
		"irq handled: irq_count=%u event_count=%u status=0x%x active=%u irq_status_before=0x%x\n",
		d->mb.irq_count, event_count, status,
		!!(status & MB_STATUS_EVENT_ACTIVE), irq_status_before);

	mb_ack_irq(&d->mb);
	irq_status_after = mb_get_irq_status(&d->mb);
	dev_info(&d->pdev->dev, "irq_status_after=0x%x\n", irq_status_after);
	
	mutex_lock(&d->lock);
	d->alarm = 1;
	mutex_unlock(&d->lock);
	wake_up_interruptible(&d->wq);

	return IRQ_HANDLED;
}

static int malm_open(struct inode *inode, struct file *fp)
{
	struct myalarmdev *malm;
	pr_info("myalarm: device opened");
	malm = container_of(inode->i_cdev, struct myalarmdev, almcdev);
	fp->private_data = malm;
	return 0;
}

static int malm_close(struct inode *inode, struct file *fp)
{
	pr_info("myalarm: device closed");
	return 0;
}

static __poll_t malm_poll(struct file *fp, struct poll_table_struct *wait)
{
	struct myalarmdev *mydev = fp->private_data;
	__poll_t mask = 0;
	poll_wait(fp, &mydev->wq, wait);
	mutex_lock(&mydev->lock);
	if(mydev->alarm){
		mask |= POLLIN | POLLRDNORM;
	}
	mutex_unlock(&mydev->lock);
	return mask;
}

static ssize_t malm_read(struct file *fp, char __user *buf, size_t count, loff_t *offset)
{
	struct myalarmdev *mydev = fp->private_data;
	char kbuf[16];
	int len = 0;
	mutex_lock(&mydev->lock);
	sprintf(kbuf, "Alarm:%d\n",mydev->alarm);
	mutex_unlock(&mydev->lock);
	len = strlen(kbuf);
	if(count < len){
		pr_err("myalarm: buffer too small");
		return -EINVAL;
	}
	if(copy_to_user(buf, kbuf, len)){
		pr_err("myalarm: copy to user error");
		return -EFAULT;
	}
	return len;
}

static ssize_t malm_write(struct file *fp, const char __user *buf, size_t count, loff_t *offset)
{
	struct myalarmdev *mydev = fp->private_data;
	int newalarm;
	int ret;
	ret = kstrtoint_from_user(buf, count, 10, &newalarm);
	if(ret){
		pr_err("myalarm: invalid input");
		return ret;
	}
	mutex_lock(&mydev->lock);
	pr_info("myalarm: read alarm value %d, new alarm value %d", mydev->alarm, newalarm);
	if(newalarm == 0 && mydev->alarm == 1){	
		mydev->alarm = 0;
	}
	mutex_unlock(&mydev->lock);
	return count;
}

struct file_operations fops = {
	.open = malm_open,
	.release = malm_close,
	.poll = malm_poll,
	.read = malm_read,
	.write = malm_write,
};

static int alarm_probe(struct platform_device *pdev)
{
	struct resource *res;
	void __iomem *base;
	int irq;
	struct myalarmdev *mydev;
	int ret;

	mydev = devm_kzalloc(&pdev->dev, sizeof(struct myalarmdev), GFP_KERNEL);
	if(mydev == NULL){
		dev_err(&pdev->dev, "allocate myalarm error");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if(NULL == res){
		dev_err(&pdev->dev, "faile to get resource");
		return -ENODEV;
	}
	mydev->pdev = pdev;
	mutex_init(&mydev->lock);

	cdev_init(&mydev->almcdev, &fops);
	ret = cdev_add(&mydev->almcdev, devnum, 1);
	if(ret){
		dev_err(&pdev->dev, "add cdev error");
		return ret;
	}
	mydev->pcdev = device_create(mycls, NULL, devnum, NULL, "myalarm0");
	if(IS_ERR(mydev->pcdev)){
		dev_err(&pdev->dev, "create device error");
		cdev_del(&mydev->almcdev);
		return PTR_ERR(mydev->pcdev);
	}

	init_waitqueue_head(&mydev->wq);

	base = devm_ioremap_resource(&pdev->dev, res);
	if( IS_ERR(base)) {
		dev_err(&pdev->dev, "ioremap error");
		cdev_del(&mydev->almcdev);
		device_destroy(mycls, devnum);
		return PTR_ERR(base);
	}
	mydev->mb.base = base;

	irq = platform_get_irq(pdev, 0);
	if(irq<0){
		dev_err(&pdev->dev, "get irq error");
		cdev_del(&mydev->almcdev);
		device_destroy(mycls, devnum);
		return irq;
	}
	mydev->mb.irq = irq;

	
	mb_ack_irq(&mydev->mb);
	ret = devm_request_threaded_irq(&pdev->dev, mydev->mb.irq,
					alarm_irq_handler, alarm_irq_thread,
					IRQF_ONESHOT, "myalarm_irq", mydev);
	if(ret){
		dev_err(&pdev->dev, "request irq error");
		cdev_del(&mydev->almcdev);
		device_destroy(mycls, devnum);
		return ret;
	}
	mb_enable_irq(&mydev->mb);

	platform_set_drvdata(pdev, mydev);

	dev_info(&pdev->dev, "alarm probe success");
	return 0;
}

static void alarm_remove(struct platform_device *pdev)
{
	struct myalarmdev *mydev = platform_get_drvdata(pdev);
	mb_disable_irq(&mydev->mb);
	dev_info(&pdev->dev, "alarm removed");
	cdev_del(&mydev->almcdev);
	device_destroy(mycls, devnum);
	return;
}

static struct platform_device_id platform_device_ids[] = {
	{
		.name = "fakebell@1f000000",
	},
	{},
};

static const struct of_device_id myalarm_of_match[] = {
	{ .compatible = "zxy,fakebell" },
	{ }
};
MODULE_DEVICE_TABLE(of, myalarm_of_match);


static struct platform_driver myalarmdrv = {
	.probe = alarm_probe,
	.remove = alarm_remove,
	.driver = {
		.name = "my_alarm_driver",
		.of_match_table = myalarm_of_match,
	},
	.id_table = platform_device_ids,
};

#ifdef DRIVER_TEST
static struct platform_device *testdev;
static struct resource myalarm_test_resources[] = {
	{
		.start = 0x1f000000,
		.end = 0x1f000fff,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = 70,
		.end = 70,
		.flags = IORESOURCE_IRQ,
	},
};
#endif

static int __init myalarminit(void)
{
	pr_info("myalarm register");
        static char clsname[] = "malm_fcls";
	int ret;
	ret = alloc_chrdev_region(&devnum, 0, MAX_DEVICE,clsname);
	if(ret){
		pr_err("alloc chrdev error");
		return ret;
	}
	
	mycls = class_create(clsname);
	if(IS_ERR(mycls)){
		pr_err("cls create error");
		unregister_chrdev_region(devnum, MAX_DEVICE);
		return PTR_ERR(mycls);
	}

	ret = platform_driver_register(&myalarmdrv);
	if (ret) {
		pr_err("platform driver register error");
		class_destroy(mycls);
		unregister_chrdev_region(devnum, MAX_DEVICE);
		return ret;
	}
#ifdef DRIVER_TEST
	testdev = platform_device_register_simple("fakebell@1f000000", 0, myalarm_test_resources, 2);
#endif
	return 0;
}

static void __exit myalarmexit(void)
{
	pr_info("myalarm unregister");
	platform_driver_unregister(&myalarmdrv);
	class_destroy(mycls);
	unregister_chrdev_region(devnum, MAX_DEVICE);
#ifdef DRIVER_TEST
	platform_device_unregister(testdev);
#endif
	return;
}

module_init(myalarminit);
module_exit(myalarmexit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxy");
MODULE_DESCRIPTION("Alarm device driver");
