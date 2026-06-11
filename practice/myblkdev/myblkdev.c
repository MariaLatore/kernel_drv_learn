#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/vmalloc.h>

#define MAX_DEV_NUM 16
#define MYBLK_DATA_SIZE (16 * PAGE_SIZE)
#define MYBLK_CTRL_SIZE PAGE_SIZE
#define MYBLK_MAP_SIZE (MYBLK_CTRL_SIZE + MYBLK_DATA_SIZE)

struct myblk_ring_ctrl {
  u32 head;
  u32 tail;
  u32 size;
  u32 overflow;
};

static struct class *mycls;
static dev_t devnum;
static DEFINE_IDA(minorconf);
struct myblkdev {
  struct platform_device *pdev;
  struct cdev mcdev;
  struct device *pcdev;
  int minor;

  void *map_area;
  struct myblk_ring_ctrl *ctrl;
  char *data;

  struct timer_list timer;

  dev_t devn;
  wait_queue_head_t wq;
  spinlock_t lock;
};

static void myblk_produce_data(struct myblkdev *mydev, const char *src,
                               size_t len) {
  unsigned long flags;
  u32 head, tail, free_space;
  u32 size = mydev->ctrl->size;

  spin_lock_irqsave(&mydev->lock, flags);

  head = mydev->ctrl->head;
  tail = mydev->ctrl->tail;

  if (head >= tail)
    free_space = size - (head - tail) - 1;
  else
    free_space = tail - head - 1;

  if (len > free_space) {
    mydev->ctrl->overflow++;
    len = free_space;
  }

  if (len > 0) {
    if (head + len <= size) {
      memcpy(mydev->data + head, src, len);
    } else {
      u32 first = size - head;
      memcpy(mydev->data + head, src, first);
      memcpy(mydev->data, src + first, len - first);
    }
    mydev->ctrl->head = (head + len) % size;
  }

  spin_unlock_irqrestore(&mydev->lock, flags);
  if (len > 0)
    wake_up_interruptible(&mydev->wq);
}

static void myblk_timer_fn(struct timer_list *t) {
  struct myblkdev *mydev = from_timer(mydev, t, timer);
  static char sample[] = "hello-from-kernel\n";

  myblk_produce_data(mydev, sample, sizeof(sample) - 1);
  mod_timer(&mydev->timer, jiffies + msecs_to_jiffies(1000));
}

static int myblkopen(struct inode *inode, struct file *fp) {
  struct myblkdev *mydev = container_of(inode->i_cdev, struct myblkdev, mcdev);
  fp->private_data = mydev;
  dev_info(&mydev->pdev->dev, "myblkdev file open\n");
  return 0;
}

static int myblkrelease(struct inode *inode, struct file *fp) {
  struct myblkdev *mydev = fp->private_data;
  dev_info(&mydev->pdev->dev, "myblkdev file release\n");
  return 0;
}

static int myblkmmap(struct file *fp, struct vm_area_struct *vma) {
  struct myblkdev *mydev = fp->private_data;
  unsigned long page_frame_num;
  int ret;
  unsigned long vsize = vma->vm_end - vma->vm_start;
  unsigned long uaddr = vma->vm_start;
  unsigned long offset = 0;
  if (vsize > MYBLK_MAP_SIZE || !PAGE_ALIGNED(vsize))
    return -EINVAL;

  if (vma->vm_pgoff != 0)
    return -EINVAL;

  while (offset < vsize) {
    page_frame_num = vmalloc_to_pfn(mydev->map_area + offset);
    ret = remap_pfn_range(vma, uaddr + offset, page_frame_num, PAGE_SIZE,
                          vma->vm_page_prot);
    if (ret)
      return ret;
    offset += PAGE_SIZE;
  }
  return 0;
}

static __poll_t myblkpoll(struct file *fp, struct poll_table_struct *wait) {
  struct myblkdev *mydev = fp->private_data;
  __poll_t mask = 0;
  poll_wait(fp, &mydev->wq, wait);

  if (mydev->ctrl->head != mydev->ctrl->tail)
    mask |= POLLIN | POLLRDNORM;
  return mask;
}

static struct file_operations fops = {
    .open = myblkopen,
    .release = myblkrelease,
    .mmap = myblkmmap,
    .poll = myblkpoll,
    .owner = THIS_MODULE,
};

static int myblkprobe(struct platform_device *pdev) {
  int ret;
  struct myblkdev *mydev = devm_kzalloc(&pdev->dev, sizeof(*mydev), GFP_KERNEL);
  if (!mydev) {
    dev_err(&pdev->dev, "kzalloc dev error\n");
    return -ENOMEM;
  }
  mydev->pdev = pdev;

  mydev->minor = ida_alloc_range(&minorconf, 0, MAX_DEV_NUM - 1, GFP_KERNEL);
  if (mydev->minor < 0) {
    dev_err(&pdev->dev, "ida alloc error\n");
    return mydev->minor;
  }

  mydev->devn = MKDEV(MAJOR(devnum), mydev->minor);

  cdev_init(&mydev->mcdev, &fops);
  ret = cdev_add(&mydev->mcdev, mydev->devn, 1);
  if (ret) {
    dev_err(&pdev->dev, "cdev add error\n");
    ida_free(&minorconf, mydev->minor);
    return ret;
  }

  mydev->pcdev =
      device_create(mycls, NULL, mydev->devn, NULL, "myblk%d", mydev->minor);
  if (IS_ERR(mydev->pcdev)) {
    dev_err(&mydev->pdev->dev, "cdev create error\n");
    cdev_del(&mydev->mcdev);
    ida_free(&minorconf, mydev->minor);
    return PTR_ERR(mydev->pcdev);
  }

  mydev->map_area = vzalloc(MYBLK_MAP_SIZE);
  if (!mydev->map_area) {
    dev_err(&mydev->pdev->dev, "shared mem alloc error\n");
    device_destroy(mycls, mydev->devn);
    cdev_del(&mydev->mcdev);
    ida_free(&minorconf, mydev->minor);
    return -ENOMEM;
  }
  mydev->ctrl = (struct myblk_ring_ctrl *)mydev->map_area;
  mydev->data = (char *)mydev->map_area + MYBLK_CTRL_SIZE;
  mydev->ctrl->size = MYBLK_DATA_SIZE;
  mydev->ctrl->overflow = 0;

  init_waitqueue_head(&mydev->wq);
  spin_lock_init(&mydev->lock);

  timer_setup(&mydev->timer, myblk_timer_fn, 0);
  mod_timer(&mydev->timer, jiffies + msecs_to_jiffies(1000));

  platform_set_drvdata(pdev, mydev);
  dev_info(&mydev->pdev->dev, "myblkdev probe success");

  return 0;
}

static void myblkremove(struct platform_device *pdev) {
  struct myblkdev *mydev = platform_get_drvdata(pdev);
  del_timer_sync(&mydev->timer);
  device_destroy(mycls, mydev->devn);
  cdev_del(&mydev->mcdev);
  ida_free(&minorconf, mydev->minor);
  vfree(mydev->map_area);
}

static struct platform_device_id myidtbl[] = {
    {.name = "testdev"},
    {},
};

static struct platform_driver myblkdrv = {
    .probe = myblkprobe,
    .remove = myblkremove,
    .driver =
        {
            .name = "myblkdrv",
        },
    .id_table = myidtbl,
};

#ifdef TESTDRIVER
static struct platform_device *testdev;
#endif

static int __init myblkinit(void) {
  int ret;
  static char clsname[] = "myblkcls";
  ret = alloc_chrdev_region(&devnum, 0, MAX_DEV_NUM, clsname);
  if (ret) {
    pr_err("alloc chrdev region error\n");
    return ret;
  }
  mycls = class_create(clsname);
  if (IS_ERR(mycls)) {
    pr_err("class create error\n");
    unregister_chrdev_region(devnum, MAX_DEV_NUM);
    return PTR_ERR(mycls);
  }
  ret = platform_driver_register(&myblkdrv);
  if (ret) {
    pr_err("platform_driver_register error\n");
    class_destroy(mycls);
    unregister_chrdev_region(devnum, MAX_DEV_NUM);
    return ret;
  }
#ifdef TESTDRIVER
  testdev = platform_device_alloc("testdev", 0);
  if (!testdev) {
    pr_err("testdev alloc error\n");
  } else {
    ret = platform_device_add(testdev);
    if (ret) {
      pr_err("testdev register error\n");
      platform_device_put(testdev);
      testdev = NULL;
    }
  }
#endif
  pr_info("myblk init success\n");
  return 0;
}

static void __exit myblkexit(void) {
#ifdef TESTDRIVER
  if (testdev)
    platform_device_unregister(testdev);
#endif
  ida_destroy(&minorconf);
  platform_driver_unregister(&myblkdrv);
  class_destroy(mycls);
  unregister_chrdev_region(devnum, MAX_DEV_NUM);
  pr_info("myblk exit\n");
}

module_init(myblkinit);
module_exit(myblkexit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ZXY");
MODULE_DESCRIPTION("myblk dev");
