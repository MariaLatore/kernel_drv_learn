#include "myfakebell.h"
#include "mybell.h"
#include "ringbuf.h"
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#define MAX_DEV_NUM 16

static dev_t devnum;
static struct class *mycls;

struct myfbdev {
  struct platform_device *pdev;
  int minor;
  dev_t devn;
  struct device *pcdev;
  struct cdev cdev;

  struct mybell_dev mybell;
  spinlock_t mybelllock;
  struct ringbuf myrb;

  wait_queue_head_t wq;
};

static int myfbopen(struct inode *inode, struct file *fp) {
  struct myfbdev *mydev = container_of(inode->i_cdev, struct myfbdev, cdev);
  fp->private_data = mydev;
  dev_info(mydev->pcdev, "myfakebell open\n");
  return 0;
}

static int myfbrelease(struct inode *inode, struct file *fp) {
  struct myfbdev *mydev = fp->private_data;
  dev_info(mydev->pcdev, "myfakebell release\n");
  return 0;
}

static long myfbioctl(struct file *fp, unsigned int nr, unsigned long param) {
  int ret = 0;
  struct myfbdev *mydev = fp->private_data;
  switch (nr) {
  case FB_GET_STATUS: {
    struct statusmsg __user *usrmsg = (struct statusmsg __user *)param;
    struct statusmsg msg;
    spin_lock(&mydev->mybelllock);
    msg.status = mb_get_status(&mydev->mybell);
    msg.threshold = mb_get_threshold(&mydev->mybell);
    msg.last_sample = mb_get_sample_value(&mydev->mybell);
    spin_unlock(&mydev->mybelllock);
    if (copy_to_user(usrmsg, &msg, sizeof(*usrmsg))) {
      dev_err(mydev->pcdev, "copy to user status msg  error\n");
      ret = -EFAULT;
    }
    break;
  }
  case FB_SET_THRESHOLD: {
    u32 newval;
    u32 __user *usrval = (u32 __user *)param;
    if (copy_from_user(&newval, usrval, sizeof(newval))) {
      dev_err(mydev->pcdev, "copy from user threshold error\n");
      ret = -EFAULT;
    } else {
      spin_lock(&mydev->mybelllock);
      mb_set_threshold(&mydev->mybell, newval);
      spin_unlock(&mydev->mybelllock);
    }
    break;
  }
  case FB_CLEAR_ALARM:
    break;
  case FB_GET_STATS: {
    struct statmsg __user *usrbufmsg = (struct statmsg __user *)param;
    struct statmsg bufmsg;
    bufmsg.bufcnt = ringbuf_getsize(&mydev->myrb);
    bufmsg.dropcnt = ringbuf_getlost(&mydev->myrb);
    bufmsg.irqcnt = 0;
    if (copy_to_user(usrbufmsg, &bufmsg, sizeof(*usrbufmsg))) {
      dev_err(mydev->pcdev, "copy to user error\n");
      ret = -EFAULT;
    }
    break;
  }
  case FB_RESET_BUFFER: {
    ringbuf_init(&mydev->myrb);
    break;
  }
  default:
    dev_err(mydev->pcdev, "invalid ioctl numver\n");
    ret = -EINVAL;
    break;
  }
  return ret;
}

static ssize_t myfbread(struct file *fp, char __user *usrbuf, size_t count,
                        loff_t *off) {
  int len;
  int ret = 0;
  int i;
  struct rbdata v;
  struct myfbdev *mydev = fp->private_data;
  int offset = 0;
  len = ringbuf_getsize(&mydev->myrb);
  if (len == 0 && !(fp->f_flags & O_NONBLOCK)) {
    ret = wait_event_interruptible(mydev->wq, ringbuf_getsize(&mydev->myrb));
    if (ret) {
      dev_err(mydev->pcdev, "error when wait\n");
      return ret;
    }
    len = ringbuf_getsize(&mydev->myrb);
  } else if (len == 0 && (fp->f_flags & O_NONBLOCK)) {
    return -EAGAIN;
  }
  if (len > count / sizeof(struct rbdata))
    len = count / sizeof(struct rbdata);
  for (i = 0; i < len; i++) {
    ret = ringbuf_pop(&mydev->myrb, &v);
    if (ret)
      break;
    if (copy_to_user(usrbuf + offset, &v, sizeof(v))) {
      dev_err(mydev->pcdev, "error read cpy to usr\n");
      ret = -EFAULT;
    }
    if (ret)
      break;
    offset += sizeof(v);
  }
  if (ret)
    return ret;

  return offset;
}

static __poll_t myfbpoll(struct file *fp, struct poll_table_struct *tbl) {
  struct myfbdev *mydev = fp->private_data;
  u32 status;
  __poll_t mask = 0;
  poll_wait(fp, &mydev->wq, tbl);
  spin_lock(&mydev->mybelllock);
  status = mb_get_status(&mydev->mybell);
  spin_unlock(&mydev->mybelllock);
  if (status & MB_STATUS_OVER_THRESHOLD)
    mask |= POLLPRI;
  if (ringbuf_getsize(&mydev->myrb))
    mask |= POLLIN | POLLRDNORM;
  return mask;
}

static struct file_operations fops = {
    .open = myfbopen,
    .release = myfbrelease,
    .read = myfbread,
    .poll = myfbpoll,
    .unlocked_ioctl = myfbioctl,
    .owner = THIS_MODULE,
};

static DEFINE_IDA(minorconf);

static irqreturn_t myirq_top(int irq, void *data) {
  struct myfbdev *mydev = data;
  u32 irqstatus;
  spin_lock(&mydev->mybelllock);
  irqstatus = mb_get_irq_status(&mydev->mybell);
  if (!(irqstatus & MB_IRQ_STATUS_PENDING)) {
    dev_info(&mydev->pdev->dev, "myirqtop none\n");
    spin_unlock(&mydev->mybelllock);
    return IRQ_NONE;
  }
  spin_unlock(&mydev->mybelllock);
  dev_info(&mydev->pdev->dev, "myirqtop\n");
  return IRQ_WAKE_THREAD;
}

static irqreturn_t myirq_bottom(int irq, void *data) {
  struct myfbdev *mydev = data;
  struct rbdata v = {0};
  spin_lock(&mydev->mybelllock);
  v.sample_value = mb_get_sample_value(&mydev->mybell);
  v.status = mb_get_status(&mydev->mybell);
  mb_ack_irq(&mydev->mybell);
  spin_unlock(&mydev->mybelllock);
  v.timestamp_ns = ktime_get_ns();

  ringbuf_push(&mydev->myrb, &v);

  wake_up_interruptible(&mydev->wq);

  dev_info(&mydev->pdev->dev,
           "myirqbottom, status 0x%x, sample data %d, ns %llu\n", v.status,
           v.sample_value, v.timestamp_ns);
  return IRQ_HANDLED;
}

static void myfakebell_init(struct myfbdev *mydev) {
  spin_lock_init(&mydev->mybelllock);
  ringbuf_init(&mydev->myrb);
  init_waitqueue_head(&mydev->wq);
}

static int myfbprobe(struct platform_device *pdev) {
  int ret;
  struct resource *res;
  struct myfbdev *mydev = kzalloc(sizeof(*mydev), GFP_KERNEL);
  if (!mydev) {
    dev_err(&pdev->dev, "mydev alloc error\n");
    return -ENOMEM;
  }
  mydev->pdev = pdev;

  // 1. create char dev for device
  cdev_init(&mydev->cdev, &fops);
  mydev->minor = ida_alloc_range(&minorconf, 0, MAX_DEV_NUM - 1, GFP_KERNEL);
  if (0 > mydev->minor) {
    dev_err(&pdev->dev, "ida alloc error\n");
    ret = -ENOMEM;
    goto ida_error;
  }
  mydev->devn = MKDEV(MAJOR(devnum), mydev->minor);

  ret = cdev_add(&mydev->cdev, mydev->devn, 1);
  if (ret) {
    dev_err(&pdev->dev, "cdev add error\n");
    goto cdevadd_error;
  }

  mydev->pcdev = device_create(mycls, NULL, mydev->devn, NULL, "myfakebell%d",
                               mydev->minor);
  if (IS_ERR(mydev->pcdev)) {
    dev_err(&pdev->dev, "cdev create error\n");
    ret = PTR_ERR(mydev->pcdev);
    goto cdevcreate_error;
  }

  // 2. ioremap
  res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!res) {
    dev_err(&pdev->dev, "device get resource error\n");
    ret = -EINVAL;
    goto res_error;
  }
  mydev->mybell.base = devm_ioremap_resource(&pdev->dev, res);
  if (IS_ERR(mydev->mybell.base)) {
    dev_err(&pdev->dev, "device ioremap error\n");
    ret = PTR_ERR(mydev->mybell.base);
    goto res_error;
  }
  myfakebell_init(mydev);
  mb_enable_irq(&mydev->mybell);

  // 3. create interrupt
  mydev->mybell.irq = platform_get_irq(pdev, 0);
  if (mydev->mybell.irq < 0) {
    dev_err(&pdev->dev, "device get irq error\n");
    ret = mydev->mybell.irq;
    goto res_error;
  }
  ret = devm_request_threaded_irq(&pdev->dev, mydev->mybell.irq, myirq_top,
                                  myirq_bottom, IRQF_ONESHOT, NULL, mydev);
  if (ret) {
    dev_err(&pdev->dev, "request irq error\n");
    goto res_error;
  }
  platform_set_drvdata(pdev, mydev);
  dev_info(&pdev->dev, "dev probe success\n");
  return 0;
res_error:
  device_destroy(mycls, mydev->devn);
cdevcreate_error:
  cdev_del(&mydev->cdev);
cdevadd_error:
  ida_free(&minorconf, mydev->minor);
ida_error:
  kfree(mydev);
  return ret;
}

static void myfbremove(struct platform_device *pdev) {
  struct myfbdev *mydev = platform_get_drvdata(pdev);
  device_destroy(mycls, mydev->devn);
  cdev_del(&mydev->cdev);
  ida_free(&minorconf, mydev->minor);
  kfree(mydev);
  dev_info(&pdev->dev, "dev remove\n");
}

static struct of_device_id idtbl[] = {
    {
        .compatible = "zxy,fakebell",
    },
    {},
};

static struct platform_driver myfbdrv = {
    .probe = myfbprobe,
    .remove = myfbremove,
    .driver =
        {
            .name = "myfakebelldriver",
            .of_match_table = idtbl,
        },
};

static int __init myfbinit(void) {
  int ret;
  static char clsname[] = "myfbclass";
  ret = alloc_chrdev_region(&devnum, 0, MAX_DEV_NUM, clsname);
  if (ret) {
    pr_err("alloc chrdev region error\n");
    return ret;
  }
  mycls = class_create(clsname);
  if (IS_ERR(mycls)) {
    pr_err("class create error\n");
    ret = PTR_ERR(mycls);
    goto class_error;
  }
  ret = platform_driver_register(&myfbdrv);
  if (ret) {
    pr_err("platform drv register error\n");
    goto pltdrv_error;
  }
  pr_info("myfakebell init success\n");
  return 0;
pltdrv_error:
  class_destroy(mycls);
class_error:
  unregister_chrdev_region(devnum, MAX_DEV_NUM);
  return ret;
}

static void __exit myfbexit(void) {
  platform_driver_unregister(&myfbdrv);
  class_destroy(mycls);
  unregister_chrdev_region(devnum, MAX_DEV_NUM);
  pr_info("myfakebell exit\n");
}

module_init(myfbinit);
module_exit(myfbexit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxy");
MODULE_DESCRIPTION("my fake bell");
