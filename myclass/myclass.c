#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/device/class.h>
#include <linux/types.h>
#include <linux/device.h>

static struct class my_class = {
	.name = "myclass",
};

static struct device *my_class_device;
static struct device mycdevice;
static dev_t my_dev_num;

static int __init my_init(void)
{
	int err;
	err = class_register(&my_class);
	if(err<0)
		pr_warn("error register %s", my_class.name);
	else
		pr_warn("successfully register %s", my_class.name);
	err = alloc_chrdev_region(&my_dev_num, 0, 1, "my_char_dev");
	if (err < 0) {
        	pr_err("Failed to allocate char dev region\n");
        	return err;
    	}
	my_class_device = device_create(&my_class, NULL, my_dev_num, (void*)&mycdevice, "myclass0");
	return err;
}

static void __exit my_cleanup(void)
{
	pr_warn("unregistering %s", my_class.name);
	device_destroy(&my_class, my_dev_num);
	class_unregister(&my_class);
	unregister_chrdev_region(my_dev_num, 1); 
}


module_init(my_init);
module_exit(my_cleanup);
MODULE_LICENSE("GPL");

