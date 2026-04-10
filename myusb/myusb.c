#include <asm/uaccess.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>

#define VENDOR_ID 0x1130
#define PRODUCT_ID 0x0202

// Private structure
struct usb_panicb {
  struct usb_device *udev;
  unsigned int button;
};

// Forward declaration
static struct usb_driver panicb_driver;

// Table of devices that work with this driver
static struct usb_device_id id_table[] = {
    {USB_DEVICE(VENDOR_ID, PRODUCT_ID)},
    {},
};
MODULE_DEVICE_TABLE(usb, id_table);

// Ask panic button for button status
static int get_panicb_button_status(struct usb_panicb *panicb_dev) {
  char *buf;
  int ret = 0;
  pr_info("get_panicb_button_status\n");

  // Allocate msg buffer
  if (!(buf = kmalloc(8, GFP_KERNEL))) {
    pr_warn("panicb: cannot alloc buf\n");
    return -1;
  }

  memset(buf, 0, 8);
  ret = usb_control_msg(panicb_dev->udev, usb_rcvctrlpipe(panicb_dev->udev, 0),
                        0x01, 0xA1, 0X300, 0X00, buf, 8, 2 * HZ);
  if (ret < 0)
    pr_warn("panicb:IN, ret = %d\n", ret);
  else
    panicb_dev->button = *buf;
  kfree(buf);
  return 0;
}

// Char device functions
static int panicb_open(struct inode *inode, struct file *file) {
  struct usb_panicb *dev;
  struct usb_interface *interface;
  int minor;
  minor = iminor(inode);

  // Get interface for device
  interface = usb_find_interface(&panicb_driver, minor);
  if (!interface)
    return -ENODEV;

  // Get private data from interface
  dev = usb_get_intfdata(interface);
  if (dev == NULL) {
    pr_warn("panicb:can't find device for minor %d\n", minor);
    return -ENODEV;
  }

  // Set to file sturcture
  file->private_data = (void *)dev;
  return 0;
}

static int panicb_release(struct inode *inode, struct file *file) { return 0; }

static long int panicb_ioctl(struct file *file, unsigned int cmd,
                             unsigned long arg) {
  struct usb_panicb *dev;
  pr_debug("panicb_ioctl\n");

  // get the dev object
  dev = file->private_data;
  if (dev == NULL)
    return -ENODEV;

  switch (cmd) {
  case 0:
    pr_info("panicb_ioctl0\n");
    if (get_panicb_button_status(dev) == 0) {
      if (copy_to_user((void *)arg, &(dev->button), sizeof(dev->button))) {
        pr_warn("panicb: copy_to_user error");
        return -EFAULT;
      }
    }
    break;
  default:
    pr_warn("panicb_ioctl(): unsupported command %d\n", cmd);
    return -EINVAL;
  }
  return 0;
}

static struct file_operations panicb_fops = {.open = panicb_open,
                                             .release = panicb_release,
                                             .unlocked_ioctl = panicb_ioctl};

// USB driver functions
static struct usb_class_driver panicb_class_driver = {
    .name = "usb/panicb", .fops = &panicb_fops, .minor_base = 0};

static int panicb_probe(struct usb_interface *interface,
                        const struct usb_device_id *id) {
  struct usb_device *udev = interface_to_usbdev(interface);
  struct usb_panicb *panicb_dev;
  int ret;
  pr_info("panicb_probe: starting\n");

  panicb_dev = kmalloc(sizeof(struct usb_panicb), GFP_KERNEL);
  if (panicb_dev == NULL) {
    dev_err(&interface->dev, "Out of memory\n");
    return -ENOMEM;
  }
  // Fill private structure and save it with usb_set_intfdata
  memset(panicb_dev, 0x00, sizeof(*panicb_dev));
  panicb_dev->udev = usb_get_dev(udev);
  panicb_dev->button = 0;
  usb_set_intfdata(interface, panicb_dev);

  ret = usb_register_dev(interface, &panicb_class_driver);
  if (ret < 0) {
    pr_warn("panicb: usb_register_dev() error\n");
    return ret;
  }

  dev_info(&interface->dev, "USB Panic Button device now attached\n");
  return 0;
}

static void panicb_disconnect(struct usb_interface *interface) {
  struct usb_panicb *dev;
  dev = usb_get_intfdata(interface);
  usb_deregister_dev(interface, &panicb_class_driver);
  usb_set_intfdata(interface, NULL);
  usb_put_dev(dev->udev);
  kfree(dev);
  dev_info(&interface->dev, "USB Panic Button now disconnected\n");
}

static struct usb_driver panicb_driver = {
    .name = "panicb",
    .probe = panicb_probe,
    .disconnect = panicb_disconnect,
    .id_table = id_table,
};

// init & exit
static int __init usb_panicb_init(void) {
  int retval = 0;
  retval = usb_register(&panicb_driver);
  if (retval)
    pr_warn("usb_register failed, Error number %d", retval);
  return retval;
}

static void __exit usb_panicb_exit(void) { usb_deregister(&panicb_driver); }
module_init(usb_panicb_init);
module_exit(usb_panicb_exit);
MODULE_AUTHOR("BPB");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Panic Button driver: no URB + char device");
