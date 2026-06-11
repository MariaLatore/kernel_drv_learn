#include <linux/cdev.h>
#include <linux/dev_printk.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

#define MAX_DEV_NUM 16

struct mydevice {
  struct platform_device *pdev;
  struct cdev mcdev;
  int minor;
  dev_t devn;

  int reference;
  struct mutex reflock;

  int config;
  bool config_opened;
  struct mutex configlock;
  struct device *pcdev;

  wait_queue_head_t wq;
};

struct mycontext {
  struct mydevice *mydev;
  int is_config;
  int privinfo;
};


static void addref(struct mydevice *mydev) {
  mutex_lock(&mydev->reflock);
  mydev->reference++;
  mutex_unlock(&mydev->reflock);
}

static int relref(struct mydevice *mydev) {
  int ref=0;
  mutex_lock(&mydev->reflock);
  if(mydev->reference == 0)
	dev_err(mydev->pcdev, "ref is alread 0\n");
  else
  	ref=--mydev->reference;
  mutex_unlock(&mydev->reflock);
  return ref;
}

static int getref(struct mydevice *mydev){
	int ref;
	mutex_lock(&mydev->reflock);
	ref = mydev->reference;
	mutex_unlock(&mydev->reflock);
	return ref;
}

static void mydevice_init(struct mydevice *mydev) {
  mutex_init(&mydev->reflock);
  mutex_init(&mydev->configlock);
  init_waitqueue_head(&mydev->wq);
}

static struct class *mycls;
static dev_t devnum;
static DEFINE_IDA(minorconf);

static int mydevopen(struct inode *inode, struct file *fp) {
  int config_is_open = 0;
  struct mydevice *mydev = container_of(inode->i_cdev, struct mydevice, mcdev);
  struct mycontext *myctx = kzalloc(sizeof(*myctx), GFP_KERNEL);
  if (!myctx) {
    dev_err(mydev->pcdev, "alloc context error\n");
    return -ENOMEM;
  }
  myctx->mydev = mydev;
  fp->private_data = myctx;

  if(fp->f_mode &FMODE_WRITE){
	mutex_lock(&mydev->configlock);
	config_is_open = mydev->config_opened;
	if(config_is_open == 0){
		mydev->config_opened = 1;
		myctx->is_config=1;
	}
	mutex_unlock(&mydev->configlock);
   }

   if(config_is_open == 1){
	dev_err(mydev->pcdev, "config is already opened\n");
	kfree(myctx);
	fp->private_data = NULL;
	return -EBUSY;
  }	
	
  addref(mydev);
  dev_info(mydev->pcdev, "mydev open\n");
  return 0;
}

static int mydevrelease(struct inode *inode, struct file *fp) {
  struct mycontext *myctx = fp->private_data;
  struct mydevice *mydev = myctx->mydev;
  if(myctx->is_config){
	mutex_lock(&mydev->configlock);
	mydev->config_opened=0;
	mutex_unlock(&mydev->configlock);
  }
  kfree(myctx);
  if(0 == relref(mydev))
	wake_up_interruptible(&mydev->wq);
  dev_info(mydev->pcdev, "mydev release\n");
  return 0;
}

static ssize_t mydevread(struct file *fp, char __user *usrbuf, size_t count,
                         loff_t *off) {
  struct mycontext *myctx = fp->private_data;
  struct mydevice *mydev = myctx->mydev;
  char buf[64];
  int config;
  int len;

  if (*off)
    return 0;
  if (count < sizeof(buf)) {
    dev_err(mydev->pcdev,
            "read buffer is not large enough, need:%ld, pass:%ld\n",
            sizeof(buf), count);
    return -EINVAL;
  }
  mutex_lock(&mydev->configlock);
  config = mydev->config;
  mutex_unlock(&mydev->configlock);

  len = snprintf(buf, sizeof(buf), "config=%d, privinfo=%d\n", config,
                 myctx->privinfo);
  dev_info(mydev->pcdev, "read %s\n", buf);
  if (copy_to_user(usrbuf, buf, len)) {
    dev_err(mydev->pcdev, "copy to user error\n");
    return -EINVAL;
  }
  *off += len;
  return len;
}

static ssize_t mydevwrite(struct file *fp, const char __user *usrbuf,
                          size_t count, loff_t *off) {
  struct mycontext *myctx = fp->private_data;
  struct mydevice *mydev = myctx->mydev;
  int len;
  int configinfo;
  char kbuf[64] = {0};
  if (*off)
    return 0;
  if(!myctx->is_config)
	return -EPERM;
  if (count > sizeof(kbuf)) {
    dev_err(mydev->pcdev, "write len is too large, write:%lu, kbuf:%lu\n",
            count, sizeof(kbuf));
    return -EINVAL;
  }
  len = copy_from_user(kbuf, usrbuf, count);
  dev_info(mydev->pcdev, "kbuf:%s\n", kbuf);
  if (len != 0) {
    dev_err(mydev->pcdev, "copy from user error\n");
  }
  len = sscanf(kbuf, "configinfo=%d", &configinfo);
  if (len != 1) {
    dev_err(mydev->pcdev, "write format error\n");
    return -EINVAL;
  }
  mutex_lock(&mydev->configlock);
  mydev->config = configinfo;
  mutex_unlock(&mydev->configlock);
  dev_info(mydev->pcdev, "set configinfo=%d\n", mydev->config);
  *off += count;
  return count;
}

static struct file_operations fops = {
    .open = mydevopen,
    .read = mydevread,
    .write = mydevwrite,
    .release = mydevrelease,
    .owner = THIS_MODULE,
};

static int mydevprobe(struct platform_device *pdev) {
  int ret;
  struct mydevice *mydev = devm_kzalloc(&pdev->dev, sizeof(*mydev), GFP_KERNEL);
  if (!mydev) {
    dev_err(&pdev->dev, "mydevice alloc error\n");
    ret = -ENOMEM;
    goto devalloc_error;
  }
  mydev->pdev = pdev;
  cdev_init(&mydev->mcdev, &fops);
  mydev->minor = ida_alloc_range(&minorconf, 0, MAX_DEV_NUM - 1, GFP_KERNEL);
  if (mydev->minor < 0) {
    dev_err(&pdev->dev, "ida alloc error\n");
    ret = mydev->minor;
    goto devalloc_error;
  }
  mydev->devn = MKDEV(MAJOR(devnum), mydev->minor);
  ret = cdev_add(&mydev->mcdev, mydev->devn, 1);
  if (ret) {
    dev_err(&pdev->dev, "cdev add error\n");
    goto cdev_add_error;
  }

  mydev->pcdev =
      device_create(mycls, NULL, mydev->devn, NULL, "mydev%d", mydev->minor);
  if (IS_ERR(mydev->pcdev)) {
    dev_err(&pdev->dev, "device mydev%d create error\n", mydev->minor);
    ret = PTR_ERR(mydev->pcdev);
    goto devcreate_error;
  }
  dev_info(&pdev->dev, "mydev%d probe success\n", mydev->minor);
  platform_set_drvdata(pdev, mydev);
  mydevice_init(mydev);
  return 0;

devcreate_error:
  cdev_del(&mydev->mcdev);
cdev_add_error:
  ida_free(&minorconf, mydev->minor);
devalloc_error:
  return ret;
}

static void mydevremove(struct platform_device *pdev) {
  struct mydevice *mydev = platform_get_drvdata(pdev);
  wait_event_interruptible(mydev->wq, 0 == getref(mydev));
  device_destroy(mycls, mydev->devn);
  cdev_del(&mydev->mcdev);
  ida_free(&minorconf, mydev->minor);
  dev_info(&pdev->dev, "mydev remove\n");
}

#ifdef TESTDRIVER
static struct platform_device *testdev;
static struct platform_device *testdev2;
#endif
static struct platform_device_id pltidtbl[] = {
    {
        .name = "testdev",
    },
    {},
};

static struct platform_driver mydevdrv = {
    .probe = mydevprobe,
    .remove = mydevremove,
    .driver =
        {
            .name = "mydevdrv",
        },
    .id_table = pltidtbl,
};

static int __init mydevinit(void) {
  int ret;
  static char clsname[] = "myclass";
  ret = alloc_chrdev_region(&devnum, 0, MAX_DEV_NUM, clsname);
  if (ret) {
    pr_err("chrdev region alloc error\n");
    return ret;
  }
  mycls = class_create(clsname);
  if (IS_ERR(mycls)) {
    pr_err("class create error\n");
    ret = PTR_ERR(mycls);
    goto class_error;
  }
  ret = platform_driver_register(&mydevdrv);
  if (ret) {
    pr_err("mydriver register error\n");
    goto drvreg_error;
  }
#ifdef TESTDRIVER
  testdev = platform_device_alloc("testdev", 0);
  if (IS_ERR(testdev)) {
    platform_device_put(testdev);
    testdev = NULL;
  } else {
    if (platform_device_add(testdev)) {
      platform_device_put(testdev);
      pr_err("testdev1 add error\n");
      testdev = NULL;
    }
  }
  testdev2 = platform_device_alloc("testdev", 2);
  if (IS_ERR(testdev2)) {
    platform_device_put(testdev2);
    testdev2 = NULL;
  } else {
    if (platform_device_add(testdev2)) {
      platform_device_put(testdev2);
      pr_err("testdev2 add error\n");
      testdev2 = NULL;
    }
  }

#endif

  pr_info("mydev init\n");
  return 0;
drvreg_error:
  class_destroy(mycls);
class_error:
  unregister_chrdev_region(devnum, MAX_DEV_NUM);
  return ret;
}

static void __exit mydevexit(void) {
#ifdef TESTDRIVER
  if (testdev)
    platform_device_unregister(testdev);
  if (testdev2)
    platform_device_unregister(testdev2);
#endif
  platform_driver_unregister(&mydevdrv);
  class_destroy(mycls);
  unregister_chrdev_region(devnum, MAX_DEV_NUM);
  ida_destroy(&minorconf);
  pr_info("mydev exit\n");
}

module_init(mydevinit);
module_exit(mydevexit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zxy");
MODULE_DESCRIPTION("mydevdriver");
