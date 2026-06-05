#include "mybell.h"
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/wait.h>

#define MAX_DEVICE 16

struct ringbuffer {
  int buffer[100];
  char timestamps[100][32]; // Optional: store timestamps for each entry
  int head;
  int tail;
  int size;
  struct mutex lock;
};

int rb_isempty(struct ringbuffer *rb);
int rb_init(struct ringbuffer *rb);
void rb_push(struct ringbuffer *rb, int value);
int rb_pop(struct ringbuffer *rb, int *value, char *timestamp);

int rb_init(struct ringbuffer *rb) {
  rb->head = 0;
  rb->tail = 0;
  rb->size = 0;
  mutex_init(&rb->lock);
  return 0;
}

void rb_push(struct ringbuffer *rb, int value) {
  struct timespec64 ts;
  ktime_get_real_ts64(&ts);
  mutex_lock(&rb->lock);
  snprintf(rb->timestamps[rb->head], 32, "%lld.%09ld\n", (s64)ts.tv_sec,
           ts.tv_nsec);
  rb->buffer[rb->head] = value;
  rb->head = (rb->head + 1) % 100;
  if (rb->size < 100) {
    rb->size++;
  } else {
    // Buffer is full, move tail to overwrite oldest data
    rb->tail = (rb->tail + 1) % 100;
  }
  mutex_unlock(&rb->lock);
}

int rb_pop(struct ringbuffer *rb, int *value, char *timestamp) {
  mutex_lock(&rb->lock);
  if (rb->size == 0) {
    mutex_unlock(&rb->lock);
    return -1; // Buffer is empty
  }
  *value = rb->buffer[rb->tail];
  if (timestamp) {
    strncpy(timestamp, rb->timestamps[rb->tail], 32);
  }
  rb->tail = (rb->tail + 1) % 100;
  rb->size--;
  mutex_unlock(&rb->lock);
  return 0;
}

int rb_isempty(struct ringbuffer *rb) {
  int empty;
  mutex_lock(&rb->lock);
  empty = (rb->size == 0);
  mutex_unlock(&rb->lock);
  return empty;
}

struct mysensordev {
  struct platform_device *pdev;
  struct mybell_dev bell;
  struct ringbuffer rb;
  struct cdev cdev;
  struct device *pcdev;
  wait_queue_head_t wq;
};

static struct class *mysensor_cls = NULL;
static dev_t devnum;

static int mysensor_open(struct inode *inode, struct file *file) {
  struct mysensordev *dev;
  dev = container_of(inode->i_cdev, struct mysensordev, cdev);
  dev_info(&dev->pdev->dev, "mysensor open\n");
  file->private_data = dev;
  return 0;
}

static int mysensor_release(struct inode *inode, struct file *file) {
  struct mysensordev *mydev = file->private_data;
  dev_info(&mydev->pdev->dev, "mysensor release\n");
  return 0;
}

static ssize_t mysensor_read(struct file *file, char __user *buf, size_t count,
                             loff_t *ppos) {
  struct mysensordev *dev = file->private_data;
  int value;
  char timestamp[32];
  int ret;
  int len;
  char output[64];
  if (file->f_flags & O_NONBLOCK) {
    if (rb_pop(&dev->rb, &value, timestamp) < 0)
      return -EAGAIN;
  } else {
    ret = wait_event_interruptible(dev->wq, !rb_isempty(&dev->rb));
    if (ret)
      return ret;

    ret = rb_pop(&dev->rb, &value, timestamp);
    if (ret < 0)
      return -EFAULT;
  }
  len = snprintf(output, sizeof(output), "Value: %d, Timestamp: %s", value,
                 timestamp);
  if (copy_to_user(buf, output, len)) {
    return -EFAULT;
  }
  return len;
}

static __poll_t mysensor_poll(struct file *file,
                              struct poll_table_struct *wait) {
  struct mysensordev *dev = file->private_data;
  int mask = 0;

  poll_wait(file, &dev->wq, wait);

  if (!rb_isempty(&dev->rb)) {
    mask |= POLLIN | POLLRDNORM; // Data is available to read
  }
  return mask;
}

static struct file_operations fops = {
    .open = mysensor_open,
    .release = mysensor_release,
    .read = mysensor_read,
    .poll = mysensor_poll,
};

static irqreturn_t mysensor_irq_handler(int irq, void *dev_id) {
  struct mysensordev *dev = dev_id;
  u32 irqst = mb_get_irq_status(&dev->bell);
  if (!(irqst & MB_IRQ_STATUS_PENDING))
    return IRQ_NONE;
  return IRQ_WAKE_THREAD;
}

static irqreturn_t mysensor_irq_thread(int irq, void *dev_id) {
  struct mysensordev *dev = dev_id;
  static int data = 0;
  u32 status = mb_get_status(&dev->bell);
  u32 event_count = mb_get_event_count(&dev->bell);
  u32 irq_status_before = mb_get_irq_status(&dev->bell);

  dev_info(&dev->pdev->dev,
           "irq handled: event_count=%u status=0x%x active=%u "
           "irq_status_before=0x%x\n",
           event_count, status, !!(status & MB_STATUS_EVENT_ACTIVE),
           irq_status_before);

  rb_push(&dev->rb, data++ % 100);
  mb_ack_irq(&dev->bell);
  u32 irq_status_after = mb_get_irq_status(&dev->bell);
  dev_info(&dev->pdev->dev, "irq_status_after=0x%x\n", irq_status_after);

  wake_up_interruptible(&dev->wq);

  return IRQ_HANDLED;
}

static int mysensor_probe(struct platform_device *pdev) {
  struct mysensordev *dev;
  int ret;
  dev = devm_kzalloc(&pdev->dev, sizeof(struct mysensordev), GFP_KERNEL);
  if (!dev) {
    dev_err(&pdev->dev, "Failed to allocate mysensor device\n");
    return -ENOMEM;
  }
  dev->pdev = pdev;

  cdev_init(&dev->cdev, &fops);
  if (cdev_add(&dev->cdev, devnum, 1)) {
    dev_err(&pdev->dev, "Failed to add cdev\n");
    return -1;
  }
  dev->pcdev = device_create(mysensor_cls, NULL, devnum, NULL, "mysensor0");
  if (IS_ERR(dev->pcdev)) {
    dev_err(&pdev->dev, "Failed to create device\n");
    cdev_del(&dev->cdev);
    return PTR_ERR(dev->pcdev);
  }
  rb_init(&dev->rb);
  init_waitqueue_head(&dev->wq);

  struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!res) {
    dev_err(&pdev->dev, "Failed to get memory resource\n");
    device_destroy(mysensor_cls, devnum);
    cdev_del(&dev->cdev);
    return -ENODEV;
  }

  dev->bell.base = devm_ioremap_resource(&pdev->dev, res);
  if (IS_ERR(dev->bell.base)) {
    dev_err(&pdev->dev, "Failed to ioremap memory\n");
    device_destroy(mysensor_cls, devnum);
    cdev_del(&dev->cdev);
    return PTR_ERR(dev->bell.base);
  }

  dev->bell.irq = platform_get_irq(pdev, 0);
  if (dev->bell.irq < 0) {
    dev_err(&pdev->dev, "Failed to get IRQ\n");
    device_destroy(mysensor_cls, devnum);
    cdev_del(&dev->cdev);
    return -ENODEV;
  }
  ret =
      devm_request_threaded_irq(&pdev->dev, dev->bell.irq, mysensor_irq_handler,
                                mysensor_irq_thread, IRQF_ONESHOT, NULL, dev);
  if (ret) {
    dev_err(&pdev->dev, "Failed to request IRQ\n");
    device_destroy(mysensor_cls, devnum);
    cdev_del(&dev->cdev);
    return ret;
  }

  dev_info(&pdev->dev, "mysensor probed successfully\n");
  mb_enable_irq(&dev->bell);
  platform_set_drvdata(pdev, dev);
  return 0;
}

static void mysensor_remove(struct platform_device *pdev) {
  struct mysensordev *dev = platform_get_drvdata(pdev);
  device_destroy(mysensor_cls, devnum);
  cdev_del(&dev->cdev);
  dev_info(&pdev->dev, "mysensor removed");
}

static const struct of_device_id mysensor_of_match[] = {
    {.compatible = "zxy,fakebell"}, {}};

static struct platform_driver mysensordrv = {
    .probe = mysensor_probe,
    .remove = mysensor_remove,
    .driver =
        {
            .name = "mysensor_driver",
            .of_match_table = mysensor_of_match,
        },
};
static int __init mysensor_init(void) {
  pr_info("mysensor register");

  alloc_chrdev_region(&devnum, 0, MAX_DEVICE, "mysensor_fcls");
  mysensor_cls = class_create("mysensor_fcls");
  if (IS_ERR(mysensor_cls)) {
    pr_err("Failed to create mysensor class\n");
    unregister_chrdev_region(devnum, MAX_DEVICE);
    return PTR_ERR(mysensor_cls);
  }

  return platform_driver_register(&mysensordrv);
}

static void __exit mysensor_exit(void) {
  class_destroy(mysensor_cls);
  unregister_chrdev_region(devnum, MAX_DEVICE);
  platform_driver_unregister(&mysensordrv);
  pr_info("mysensor unregister");
}

module_init(mysensor_init);
module_exit(mysensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ZXY");
MODULE_DESCRIPTION("A simple mysensor driver");
