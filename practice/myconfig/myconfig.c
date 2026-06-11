#include <linux/cdev.h>
#include <linux/dev_printk.h>
#include <linux/device.h>
#include <linux/idr.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>

#define MAX_DEV_NUM 16

struct myconfdev {
  struct platform_device *pdev;
  struct cdev mcdev;
  struct device *pcdev;
  int minor;
  int config;
  dev_t devt;
};

static struct class *mycls;
static dev_t devnum;

static ssize_t config_show(struct device *dev, struct device_attribute *atrr,
                           char *buf) {
  struct myconfdev *mydev = dev_get_drvdata(dev);
  return sysfs_emit(buf, "%d\n", mydev->config);
}

static ssize_t config_store(struct device *dev, struct device_attribute *attr,
                            const char *buf, size_t count) {
  struct myconfdev *mydev = dev_get_drvdata(dev);
  int val;
  int ret;

  ret = kstrtoint(buf, 0, &val);
  if (ret)
    return ret;

  mydev->config = val;
  return count;
}

static ssize_t status_show(struct device *dev, struct device_attribute *atrr,
                           char *buf) {
  struct myconfdev *mydev = dev_get_drvdata(dev);
  return sysfs_emit(buf, "%d\n", mydev->minor);
}

static DEVICE_ATTR_RW(config);
static DEVICE_ATTR_RO(status);

static int configopen(struct inode *pinode, struct file *fp) {
  struct myconfdev *mydev =
      container_of(pinode->i_cdev, struct myconfdev, mcdev);
  fp->private_data = mydev;
  dev_info(&mydev->pdev->dev, "myconfdev opened");
  return 0;
}

static int configrelease(struct inode *pinode, struct file *fp) {
  struct myconfdev *mydev = fp->private_data;
  dev_info(&mydev->pdev->dev, "myconfdev closeed");
  return 0;
}

static ssize_t configwrite(struct file *fp, const char __user *buf,
                           size_t count, loff_t *offset) {
  char kbuf[1024] = {0};
  int conf;
  int ret;
  int len = min(count, sizeof(kbuf) - 1);
  struct myconfdev *mydev = fp->private_data;
  ret = copy_from_user(kbuf, buf, len);
  if (ret != 0) {
    dev_err(&mydev->pdev->dev, "copy from user error");
    return -EFAULT;
  }
  kbuf[len] = 0;
  if (sscanf(kbuf, "config=%d", &conf) != 1) {
    return -EFAULT;
  }
  mydev->config = conf;
  return count;
}

static ssize_t configread(struct file *fp, char __user *buf, size_t count,
                          loff_t *offset) {
  char kbuf[1024] = {0};
  int ret;
  int len;
  struct myconfdev *mydev = fp->private_data;
  len = snprintf(kbuf, sizeof(kbuf), "config=%d\n", mydev->config);
  if (*offset >= len)
    return 0;
  if (count > len - *offset)
    count = len - *offset;
  ret = copy_to_user(buf, kbuf + *offset, count);
  if (ret) {
    dev_err(&mydev->pdev->dev, "copy to user error");
    return -EFAULT;
  }
  *offset += count;
  return count;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = configopen,
    .release = configrelease,
    .read = configread,
    .write = configwrite,
};

static DEFINE_IDA(myconfig_ida);

static int myconfigprobe(struct platform_device *pdev) {
  struct myconfdev *mydev;
  int ret;
  mydev = devm_kzalloc(&pdev->dev, sizeof(*mydev), GFP_KERNEL);
  if (!mydev) {
    dev_err(&pdev->dev, "mydev alloc error");
    return -ENOMEM;
  }
  mydev->pdev = pdev;

  mydev->minor = ida_alloc_range(&myconfig_ida, 0, MAX_DEV_NUM - 1, GFP_KERNEL);
  if (mydev->minor < 0) {
    dev_err(&pdev->dev, "failed to alloc minor\n");
    return mydev->minor;
  }
  mydev->devt = MKDEV(MAJOR(devnum), mydev->minor);

  cdev_init(&mydev->mcdev, &fops);
  ret = cdev_add(&mydev->mcdev, mydev->devt, 1);
  if (ret) {
    dev_err(&pdev->dev, "cdev add error");
    ida_free(&myconfig_ida, mydev->minor);
    return ret;
  }
  mydev->pcdev = device_create(mycls, &pdev->dev, mydev->devt, NULL,
                               "myconfig%d", mydev->minor);
  if (IS_ERR(mydev->pcdev)) {
    dev_err(&pdev->dev, "cdev create error");
    ida_free(&myconfig_ida, mydev->minor);
    cdev_del(&mydev->mcdev);
    return PTR_ERR(mydev->pcdev);
  }
  dev_set_drvdata(mydev->pcdev, mydev);
  ret = device_create_file(mydev->pcdev, &dev_attr_config);
  if (ret) {
    dev_err(&pdev->dev, "create sysfs file error\n");
    device_destroy(mycls, mydev->devt);
    cdev_del(&mydev->mcdev);
    ida_free(&myconfig_ida, mydev->minor);
    return ret;
  }
  ret = device_create_file(mydev->pcdev, &dev_attr_status);
  if (ret) {
    device_remove_file(mydev->pcdev, &dev_attr_config);
    dev_err(&pdev->dev, "create sysfs file error\n");
    device_destroy(mycls, mydev->devt);
    cdev_del(&mydev->mcdev);
    ida_free(&myconfig_ida, mydev->minor);
    return ret;
  }

  platform_set_drvdata(pdev, mydev);
  dev_info(&pdev->dev, "probe success");
  return 0;
}

static void myconfigremove(struct platform_device *pdev) {
  struct myconfdev *mydev = platform_get_drvdata(pdev);
  device_remove_file(mydev->pcdev, &dev_attr_config);
  device_remove_file(mydev->pcdev, &dev_attr_status);
  device_destroy(mycls, mydev->devt);
  cdev_del(&mydev->mcdev);
  ida_free(&myconfig_ida, mydev->minor);
}

static const struct platform_device_id pltdevtbl[] = {
    {
        .name = "testdev",
    },
    {},
};
MODULE_DEVICE_TABLE(platform, pltdevtbl);

static const struct of_device_id ofdevtbl[] = {
    {
        .compatible = "zxy,fakebell",
    },
    {},
};
MODULE_DEVICE_TABLE(of, ofdevtbl);

struct platform_driver myconfigdrv = {
    .probe = myconfigprobe,
    .remove = myconfigremove,
    .driver =
        {
            .name = "myconfig",
            .of_match_table = ofdevtbl,
        },
    .id_table = pltdevtbl,
};

#ifdef TESTDRIVER
static struct platform_device *testdev;
static struct platform_device *testdev2;
#endif

static int __init myintfinit(void) {
  int ret;
  static char clsname[] = "myconfigclass";
  ret = alloc_chrdev_region(&devnum, 0, MAX_DEV_NUM, clsname);
  if (ret) {
    pr_err("alloc chrdev error");
    return ret;
  }

  mycls = class_create(clsname);
  if (IS_ERR(mycls)) {
    pr_err("class create error");
    unregister_chrdev_region(devnum, MAX_DEV_NUM);
    return PTR_ERR(mycls);
  }

  ret = platform_driver_register(&myconfigdrv);
  if (ret) {
    pr_err("platform driver register error");
    class_destroy(mycls);
    unregister_chrdev_region(devnum, MAX_DEV_NUM);
    return ret;
  }

#ifdef TESTDRIVER
  testdev = platform_device_alloc("testdev", 0);
  if (!testdev) {
    pr_err("testdev alloc error");
  } else {
    ret = platform_device_add(testdev);
    if (ret) {
      platform_device_put(testdev);
      testdev = NULL;
      pr_err("platformdevice register error");
    }
  }
  testdev2 = platform_device_alloc("testdev", 1);
  if (!testdev2) {
    pr_err("testdev alloc error");
  } else {
    ret = platform_device_add(testdev2);
    if (ret) {
      platform_device_put(testdev);
      testdev = NULL;
      pr_err("platformdevice2 register error");
    }
  }
#endif
  pr_info("myconfig init success");
  return 0;
}

static void __exit myintfexit(void) {
#ifdef TESTDRIVER
  if (testdev)
    platform_device_unregister(testdev);
  if (testdev2)
    platform_device_unregister(testdev2);
#endif
  platform_driver_unregister(&myconfigdrv);
  class_destroy(mycls);
  unregister_chrdev_region(devnum, MAX_DEV_NUM);
  ida_destroy(&myconfig_ida);
  pr_info("myconfig exit");
}

module_init(myintfinit);
module_exit(myintfexit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ZXY");
MODULE_DESCRIPTION("my config driver");
