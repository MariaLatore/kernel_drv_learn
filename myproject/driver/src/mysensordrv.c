#include "fakesensor.h"
#include "mysensor_ioctl.h"
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/uaccess.h>

#define CLASS_NAME "mysensor_class"
#define CDEV_NAME "mychardev"
#define DRIVER_NAME "mysensordrv"
#define DEVCOUNT 4
#define SENSOR_CDEV_NAME "mysensor%d"
typedef struct {
  struct platform_device *pdev;
  struct cdev cdev;
  struct device *pcdev;
  struct mysensor_status stat;
  dev_t devnum;
  wait_queue_head_t wq;
} mysensor_t;

static int avaliable_minor[DEVCOUNT] = {0};
static struct class *mysscls = NULL;
dev_t chardevnum = 0;
static int devcount = 0;
static struct file_operations fops;

static int get_minor(void) {
  int i;
  for (i = 0; i < DEVCOUNT; i++) {
    if (avaliable_minor[i] == 0) {
      avaliable_minor[i] = 1;
      devcount++;
      return i;
    }
  }
  return -1;
}

static void return_minor(int i) {
  if (i >= DEVCOUNT)
    return;
  avaliable_minor[i] = 0;
  devcount--;
  return;
}

static int myssdrv_probe(struct platform_device *pdev) {
  int major = MAJOR(chardevnum);
  int minor = get_minor();
  pr_info("myssdrv: probing");
  if (minor == -1) {
    pr_err("myssdrv: devcount %d, max %d", devcount, DEVCOUNT);
    return -EINVAL;
  }
  mysensor_t *newsensor = (mysensor_t *)kzalloc(sizeof(mysensor_t), GFP_KERNEL);
  if (NULL == newsensor) {
    return_minor(minor);
    return -ENOMEM;
  }
  newsensor->pdev = pdev;

  // create char dev for the device
  cdev_init(&newsensor->cdev, &fops);
  newsensor->devnum = MKDEV(major, minor);
  if (cdev_add(&newsensor->cdev, newsensor->devnum, 1)) {
    pr_err("myssdrv: Cannot add the devicei 0x%x to the system",
           newsensor->devnum);
    return_minor(minor);
    kfree(newsensor);
    return -ENOMEM;
  }

  newsensor->pcdev = device_create(mysscls, NULL, newsensor->devnum, NULL,
                                   "%s%d", CDEV_NAME, minor);
  if (IS_ERR(newsensor->pcdev)) {
    pr_err("myssdrv: cdev device_create error");
    cdev_del(&newsensor->cdev);
    return_minor(minor);
    kfree(newsensor);
    return -ENOMEM;
  }

  pr_info("myssdrv: probing finished, create /dev/%s%d", CDEV_NAME, minor);
  init_tempstat(&newsensor->stat);
  init_waitqueue_head(&newsensor->wq);

  platform_set_drvdata(pdev, newsensor);
  return 0;
}

static void myssdrv_remove(struct platform_device *pdev) {
  mysensor_t *mysensor = platform_get_drvdata(pdev);
  pr_info("myssdrv: devnum 0x%x removing", mysensor->devnum);
  cdev_del(&mysensor->cdev);
  return_minor(MINOR(mysensor->devnum));
  device_destroy(mysscls, mysensor->devnum);
  kfree(mysensor);
}

static const struct platform_device_id platform_device_ids[] = {
    {
        .name = "zxyfakesensor",
    },
    {},
};

static const struct of_device_id mysensor_dfs_match[] = {
    {.compatible = "zxy,fakesensor"},
    {},
};
MODULE_DEVICE_TABLE(of, mysensor_dfs_match);

static struct platform_driver mysensor_driver = {
    .probe = myssdrv_probe,
    .remove = myssdrv_remove,
    .driver =
        {
            .name = DRIVER_NAME,
            .of_match_table = mysensor_dfs_match,
        },
    .id_table = platform_device_ids,
};

static int device_open(struct inode *inode, struct file *fp) {
  mysensor_t *mysensor = container_of(inode->i_cdev, mysensor_t, cdev);
  pr_info("myssdrv: opened file name:%s, major %d, minor: %d",
          fp->f_path.dentry->d_name.name, MAJOR(mysensor->devnum),
          MINOR(mysensor->devnum));
  fp->private_data = mysensor;

  return 0;
}

static int device_close(struct inode *inode, struct file *fp) {
  mysensor_t *mysensor = (mysensor_t *)fp->private_data;
  pr_info("myssdrv: close file name:%s, major %d, minor: %d",
          fp->f_path.dentry->d_name.name, MAJOR(mysensor->devnum),
          MINOR(mysensor->devnum));
  fp->private_data = NULL;
  return 0;
}

static ssize_t device_read(struct file *file, char __user *buf, size_t size,
                           loff_t *off) {
  pr_info("myssdrv: device read");
  int bytes_read = 0;
  mysensor_t *mysensor = file->private_data;

  static char message[128] = {0};
  sprintf(message, "temp=%d humidity=%d alarm=%d\n", get_temp(&mysensor->stat),
          get_hum(&mysensor->stat), get_alm(&mysensor->stat));
  char *message_ptr = message;
  if (!*(message + *off)) {
    *off = 0;
    return 0;
  }
  message_ptr += *off;
  while (size && *message_ptr) {
    put_user(*(message_ptr++), buf++);
    size--;
    bytes_read++;
  }

  pr_info("Read %d bytes, %ld left\n", bytes_read, size);
  *off += bytes_read;
  return bytes_read;
}

static void cpy2kernel(void *to, const void __user *from, unsigned long n) {
  unsigned long ret = copy_from_user(to, from, n);
  if (ret != n)
    pr_warn("copy_from_user: ret %lu, n %lu", ret, n);
  return;
}

static void cpy2user(void __user *to, const void *from, unsigned long n) {
  unsigned long ret = copy_to_user(to, from, n);
  if (ret != n)
    pr_warn("copy_to_user: ret %lu, n %lu", ret, n);
  return;
}

static long device_unlocked_ioctl(struct file *fp, unsigned int ioctlnum,
                                  unsigned long ioctlparam) {
  int new = 0;
  mysensor_t *mysensor = fp->private_data;
  switch (ioctlnum) {
  case MYSENSOR_IOC_GET_TEMP:
    int temp = get_temp(&mysensor->stat);
    cpy2user((void __user *)ioctlparam, &temp, sizeof(temp));
    break;
  case MYSENSOR_IOC_GET_HUMIDITY:
    int hum = get_hum(&mysensor->stat);
    cpy2user((void __user *)ioctlparam, &hum, sizeof(hum));
    break;
  case MYSENSOR_IOC_GET_THRESHOLD:
    int lim = get_lim(&mysensor->stat);
    cpy2user((void __user *)ioctlparam, &lim, sizeof(lim));
    break;
  case MYSENSOR_IOC_GET_ALARM:
    int alm = get_alm(&mysensor->stat);
    cpy2user((void __user *)ioctlparam, &alm, sizeof(alm));
    break;
  case MYSENSOR_IOC_GET_STATUS:
    cpy2user((void __user *)ioctlparam, &mysensor->stat,
             sizeof(mysensor->stat));
    break;
  case MYSENSOR_IOC_SET_TEMP:
    new = 0;
    cpy2kernel(&new, (void __user *)ioctlparam, sizeof(new));
    pr_info("set temp from %d, to %d", set_temp(&mysensor->stat, new), new);
    if (get_alm(&mysensor->stat))
      wake_up_interruptible(&mysensor->wq);
    break;
  case MYSENSOR_IOC_SET_HUMIDITY:
    new = 0;
    cpy2kernel(&new, (void __user *)ioctlparam, sizeof(new));
    pr_info("set humidity from %d, to %d", set_hum(&mysensor->stat, new), new);
    break;
  case MYSENSOR_IOC_SET_THRESHOLD:
    new = 0;
    cpy2kernel(&new, (void __user *)ioctlparam, sizeof(new));
    pr_info("set temp from %d, to %d", set_lim(&mysensor->stat, new), new);
    if (get_alm(&mysensor->stat))
      wake_up_interruptible(&mysensor->wq);
    break;
  case MYSENSOR_IOC_CLEAR_ALARM:
    pr_info("clear alram from %d, to %d", set_alm(&mysensor->stat, 0), 0);
    break;
  case MYSENSOR_IOC_TRIGGER_ALARM:
    pr_info("trigger alram from %d, to %d", set_alm(&mysensor->stat, 1), 1);
    wake_up_interruptible(&mysensor->wq);
    break;
  default:
    pr_err("unknown ioctl operation: %d", ioctlnum);
    return -EINVAL;
  }
  return 0;
}

static __poll_t device_poll(struct file *fp, struct poll_table_struct *wait) {
  mysensor_t *mysensor = fp->private_data;
  __poll_t mask = 0;
  poll_wait(fp, &mysensor->wq, wait);
  if (get_alm(&mysensor->stat))
    mask |= POLLIN | POLLRDNORM;
  return mask;
}

static struct file_operations fops = {
    .open = device_open,
    .release = device_close,
    .read = device_read,
    .unlocked_ioctl = device_unlocked_ioctl,
    .poll = device_poll,
};

#ifdef DRIVER_TEST
struct platform_device *testdev;
struct platform_device *testdev2;
static void test_init(void) {
  // use platform_device_ids to match and auto probe
  testdev = platform_device_register_simple("zxyfakesensor", 0, NULL, 0);
  testdev2 = platform_device_register_simple("zxyfakesensor", 1, NULL, 0);
}

static void test_exit(void) {
  platform_device_unregister(testdev);
  platform_device_unregister(testdev2);
}
#endif

static char *mysscls_devnode(const struct device *dev, umode_t *mode) {
  if (mode)
    *mode = 0666;
  return NULL;
}

static int __init myssdrv_init(void) {
  pr_info("myssdrv: register");
  int ret;

  /* Allocating Major number */
  if ((alloc_chrdev_region(&chardevnum, devcount /*should be 0*/, DEVCOUNT,
                           CDEV_NAME)) < 0) {
    pr_err("myssdrv: Cannot allocate major number\n");
    return -1;
  }
  pr_info("myssdrv: Major = %d Minor = %d \n", MAJOR(chardevnum),
          MINOR(chardevnum));

  /* create cdev class */
  mysscls = class_create(CLASS_NAME);
  if (IS_ERR(mysscls)) {
    pr_err("myssdrv: class create fail");
    goto r_class;
  }

  mysscls->devnode = mysscls_devnode;
  ret = platform_driver_register(&mysensor_driver);
  if (ret) {
    pr_err("myssdrv: register driver error");
    goto r_driver;
  }
#ifdef DRIVER_TEST
  test_init();
#endif
  pr_info("myssdrv: init success");
  return 0;
r_driver:
  class_destroy(mysscls);
r_class:
  unregister_chrdev_region(chardevnum, DEVCOUNT);
  return -1;
}

static void __exit myssdrv_exit(void) {
  pr_info("myssdrv: exit");
#ifdef DRIVER_TEST
  test_exit();
#endif
  class_destroy(mysscls);
  platform_driver_unregister(&mysensor_driver);
  unregister_chrdev_region(chardevnum, DEVCOUNT);
}
module_init(myssdrv_init);
module_exit(myssdrv_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxy");
