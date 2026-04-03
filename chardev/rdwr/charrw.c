#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/minmax.h>

#define mem_size 1024

dev_t dev = 0;
static struct class *cls;
static struct cdev mycdev;
uint8_t *kernel_buffer;

/*
 * Function Prototypes
 */
static int __init mycdev_init (void);
static void __exit mycdev_exit (void);
static int mycdev_open (struct inode *inode, struct file *file);
static int mycdev_release (struct inode *inode, struct file *file);
static ssize_t mycdev_read (struct file *filp, char __user * buf, size_t len,
			    loff_t * off);
static ssize_t mycdev_write (struct file *filp, const char *buf, size_t len,
			     loff_t * off);

/*
 * File Operations structure
 */
static struct file_operations fops = {
  .owner = THIS_MODULE,
  .read = mycdev_read,
  .write = mycdev_write,
  .open = mycdev_open,
  .release = mycdev_release,
};

/*
 * This function will be called when we open the DEvice file
 */
static int
mycdev_open (struct inode *inode, struct file *file)
{
  pr_info ("Device File Opened...!!!\n");
  return 0;
}

/*
 * This function will be called when we close the Device file
 */
static int
mycdev_release (struct inode *inode, struct file *file)
{
  pr_info ("Device File Closed...!!!\n");
  return 0;
}

/*
 * This function will be called when we read the Device file
 */
static ssize_t
mycdev_read (struct file *filp, char __user *buf, size_t len, loff_t *offset)
{
  //Copy the data from the kernel space to the user-space
  size_t length = strlen (kernel_buffer);
  int buffer_size = 0;
  if (*offset || length == 0) //if offset is not zero, means we read it twice, we only read once.
    {
      pr_debug ("mycdev_read:END\n");
      *offset = 0;
      return 0;
    }
  buffer_size = min (len, length);
  if (copy_to_user (buf, kernel_buffer, buffer_size))
    {
      pr_err ("Data Read: Err!\n");
      return -EFAULT;
    }
  *offset += buffer_size;

  pr_info ("Data Read:read %d bytes Done!\n", buffer_size);
  return buffer_size;
}

/*
 * This function will be called when we write the DEvice file
 */
static ssize_t
mycdev_write (struct file *filp, const char __user *buf, size_t len,
	      loff_t *off)
{
  int buffer_size = min (len, mem_size);
  //Copy the data to kernel space from the user-space
  if (copy_from_user (kernel_buffer, buf, buffer_size))
    {
      pr_err ("Data Write: Err!\n");
      return -EFAULT;
    }
  pr_info ("Data write: Done!\n");
  return buffer_size;
}

/*
 * Module init function
 */
static int __init
mycdev_init (void)
{
  /* Allocating Major number */
  if ((alloc_chrdev_region (&dev, 0, 1, "mycdev")) < 0)
    {
      pr_info ("Cannot allocate major number\n");
      return -1;
    }
  pr_info ("Major = %d Minor = %d \n", MAJOR (dev), MINOR (dev));

  /* Creating cdev structure */
  cdev_init (&mycdev, &fops);

  /* Adding character device to the system */
  if ((cdev_add (&mycdev, dev, 1)) < 0)
    {
      pr_info ("Cannot add the device to the system");
      goto r_class;
    }

  /* Creating struct class */
  if (IS_ERR (cls = class_create ("mycdevclass")))
    {
      pr_info ("Cannot create the struct class\n");
      goto r_class;
    }

  /* Creating device */
  if (IS_ERR (device_create (cls, NULL, dev, NULL, "mycdev0")))
    {
      pr_info ("Cannot create the DEvice 1\n");
      goto r_device;
    }

  /* Creating physical memory */
  if ((kernel_buffer = kmalloc (mem_size, GFP_KERNEL)) == 0)
    {
      pr_info ("Cannot allocate memory in kernel\n");
      goto r_device;
    }

  memset (kernel_buffer, 0, mem_size);
  strcpy (kernel_buffer, "Hello world");

  pr_info ("Device Driver Insert...Done!!!\n");
  return 0;

r_device:
  class_destroy (cls);
r_class:
  unregister_chrdev_region (dev, 1);
  return -1;
}

/*
 * Module exit function
 */
static void __exit
mycdev_exit (void)
{
  kfree (kernel_buffer);
  device_destroy (cls, dev);
  class_destroy (cls);
  cdev_del (&mycdev);
  unregister_chrdev_region (dev, 1);
  pr_info ("Device Driver Remove...Done!!!\n");
}

module_init (mycdev_init);
module_exit (mycdev_exit);
MODULE_LICENSE ("GPL");
