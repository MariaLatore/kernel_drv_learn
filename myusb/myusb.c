#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/spinlock.h>

#define VENDOR_ID 0x1130
#define PRODUCT_ID 0x0202
#define REPORT_SIZE 8

struct usb_panicb
{
  struct usb_device *udev;
  struct usb_interface *interface;

  unsigned char int_in_ep;
  unsigned char *int_in_buffer;
  size_t int_in_size;
  struct urb *int_in_urb;

  unsigned char last_report[REPORT_SIZE];
  size_t last_report_len;
  unsigned int button;

  spinlock_t lock;

};

static struct usb_driver panicb_driver;

static struct usb_device_id id_table[] = {
  {USB_DEVICE (VENDOR_ID, PRODUCT_ID)},
  {}
};

MODULE_DEVICE_TABLE (usb, id_table);

static void
panicb_int_callback (struct urb *urb)
{
  struct usb_panicb *dev = urb->context;
  unsigned long flags;
  int ret;

  if (!dev)
    return;

  pr_info ("panicb: callback status=%d actual_length=%d\n",
	   urb->status, urb->actual_length);

  switch (urb->status)
    {
    case 0:
      if (urb->actual_length == 0)
	break;

      spin_lock_irqsave (&dev->lock, flags);

      dev->last_report_len = min_t (size_t, urb->actual_length, REPORT_SIZE);
      memset (dev->last_report, 0, REPORT_SIZE);
      memcpy (dev->last_report, dev->int_in_buffer, dev->last_report_len);

      if (dev->last_report_len > 0)
	dev->button = dev->last_report[0];
      else
	dev->button = 0;

      spin_unlock_irqrestore (&dev->lock, flags);

      pr_info ("panicb: received %zu bytes, button=%u\n",
	       dev->last_report_len, dev->button);
      break;

    case -ENOENT:
    case -ECONNRESET:
    case -ESHUTDOWN:
      return;

    default:
      pr_warn ("panicb: urb status=%d\n", urb->status);
      break;
    }

  ret = usb_submit_urb (urb, GFP_ATOMIC);
  if (ret)
    pr_warn ("panicb: failed to resubmit urb: %d\n", ret);

}

static int
panicb_open (struct inode *inode, struct file *file)
{
  struct usb_panicb *dev;
  struct usb_interface *interface;
  int minor;

  minor = iminor (inode);

  interface = usb_find_interface (&panicb_driver, minor);
  if (!interface)
    return -ENODEV;

  dev = usb_get_intfdata (interface);
  if (!dev)
    {
      pr_warn ("panicb: can't find device for minor %d\n", minor);
      return -ENODEV;
    }

  file->private_data = dev;
  return 0;

}

static int
panicb_release (struct inode *inode, struct file *file)
{
  return 0;
}

static long
panicb_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
  struct usb_panicb *dev;
  unsigned int button;
  unsigned long flags;

  dev = file->private_data;
  if (!dev)
    return -ENODEV;

  switch (cmd)
    {
    case 0:
      spin_lock_irqsave (&dev->lock, flags);
      button = dev->button;
      spin_unlock_irqrestore (&dev->lock, flags);
      pr_info ("panic_ioctl: returning button=%u\n", button);

      if (copy_to_user ((void __user *) arg, &button, sizeof (button)))
	{
	  pr_warn ("panicb: copy_to_user error\n");
	  return -EFAULT;
	}
      return 0;

    default:
      pr_warn ("panicb_ioctl(): unsupported command %d\n", cmd);
      return -EINVAL;
    }

}

static const struct file_operations panicb_fops = {
  .owner = THIS_MODULE,
  .open = panicb_open,
  .release = panicb_release,
  .unlocked_ioctl = panicb_ioctl,
};

static struct usb_class_driver panicb_class_driver = {
  .name = "wowpanicb%d",
  .fops = &panicb_fops,
  .minor_base = 0,
};


static int
panicb_probe (struct usb_interface *interface, const struct usb_device_id *id)
{
  struct usb_device *udev = interface_to_usbdev (interface);
  struct usb_host_interface *iface_desc;
  struct usb_endpoint_descriptor *endpoint = NULL;
  struct usb_panicb *panicb_dev;
  int i;
  int ret;

  pr_info ("panicb_probe: starting\n");

  panicb_dev = kzalloc (sizeof (*panicb_dev), GFP_KERNEL);
  if (!panicb_dev)
    {
      dev_err (&interface->dev, "Out of memory\n");
      return -ENOMEM;
    }

  panicb_dev->udev = usb_get_dev (udev);
  panicb_dev->interface = interface;
  spin_lock_init (&panicb_dev->lock);

  iface_desc = interface->cur_altsetting;

  for (i = 0; i < iface_desc->desc.bNumEndpoints; i++)
    {
      struct usb_endpoint_descriptor *ep;

      ep = &iface_desc->endpoint[i].desc;

      pr_info
	("panicb: endpoint[%d] addr=0x%02x attr=0x%02x maxp=%u interval=%u\n",
	 i, ep->bEndpointAddress, ep->bmAttributes, usb_endpoint_maxp (ep),
	 ep->bInterval);

      if (usb_endpoint_is_int_in (ep))
	{
	  endpoint = ep;
	  panicb_dev->int_in_ep = ep->bEndpointAddress;
	  panicb_dev->int_in_size = usb_endpoint_maxp (ep);

	  pr_info ("panicb: found interrupt IN endpoint 0x%02x, size=%u\n",
		   panicb_dev->int_in_ep, (unsigned) panicb_dev->int_in_size);
	  break;
	}
    }

  if (!endpoint)
    {
      dev_err (&interface->dev, "No interrupt IN endpoint found\n");
      ret = -ENODEV;
      goto error;
    }

  if (panicb_dev->int_in_size < REPORT_SIZE)
    panicb_dev->int_in_size = REPORT_SIZE;

  panicb_dev->int_in_buffer = kmalloc (panicb_dev->int_in_size, GFP_KERNEL);
  if (!panicb_dev->int_in_buffer)
    {
      ret = -ENOMEM;
      goto error;
    }

  panicb_dev->int_in_urb = usb_alloc_urb (0, GFP_KERNEL);
  if (!panicb_dev->int_in_urb)
    {
      ret = -ENOMEM;
      goto error;
    }

  usb_fill_int_urb (panicb_dev->int_in_urb,
		    panicb_dev->udev,
		    usb_rcvintpipe (panicb_dev->udev, panicb_dev->int_in_ep),
		    panicb_dev->int_in_buffer,
		    panicb_dev->int_in_size,
		    panicb_int_callback, panicb_dev, endpoint->bInterval);

  usb_set_intfdata (interface, panicb_dev);

  ret = usb_register_dev (interface, &panicb_class_driver);
  if (ret < 0)
    {
      pr_warn ("panicb: usb_register_dev() error\n");
      goto error;
    }

  pr_info ("panicb: submitting interrupt urb\n");
  ret = usb_submit_urb (panicb_dev->int_in_urb, GFP_KERNEL);
  pr_info ("panicb: usb_submit_urb ret=%d\n", ret);
  if (ret)
    {
      usb_deregister_dev (interface, &panicb_class_driver);
      goto error;
    }

  dev_info (&interface->dev, "USB Panic Button device now attached\n");
  return 0;

error:
  if (panicb_dev)
    {
      usb_free_urb (panicb_dev->int_in_urb);
      kfree (panicb_dev->int_in_buffer);
      usb_put_dev (panicb_dev->udev);
      kfree (panicb_dev);
    }
  return ret;
}

static void
panicb_disconnect (struct usb_interface *interface)
{
  struct usb_panicb *dev;

  dev = usb_get_intfdata (interface);
  usb_set_intfdata (interface, NULL);

  usb_deregister_dev (interface, &panicb_class_driver);

  if (dev)
    {
      usb_kill_urb (dev->int_in_urb);
      usb_free_urb (dev->int_in_urb);
      kfree (dev->int_in_buffer);
      usb_put_dev (dev->udev);
      kfree (dev);
    }

  dev_info (&interface->dev, "USB Panic Button now disconnected\n");

}

static struct usb_driver panicb_driver = {
  .name = "panicb",
  .probe = panicb_probe,
  .disconnect = panicb_disconnect,
  .id_table = id_table,
};

static int __init
usb_panicb_init (void)
{
  int retval;

  retval = usb_register (&panicb_driver);
  if (retval)
    pr_warn ("usb_register failed, Error number %d\n", retval);

  return retval;

}

static void __exit
usb_panicb_exit (void)
{
  usb_deregister (&panicb_driver);
}

module_init (usb_panicb_init);
module_exit (usb_panicb_exit);

MODULE_AUTHOR ("BPB");
MODULE_LICENSE ("GPL");
MODULE_DESCRIPTION ("Panic Button driver: interrupt IN + char device");
