#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include "../my_bus_raw.h"

static struct device my_device;

static int __init mydev_init(void)
{
	int err;
	my_device.init_name = "xiaoyzh3";
	err = my_bus_register_device(&my_device);
	if(err < 0)
		pr_err("my bus register device:%s failed\n", my_device.init_name);
	return err;
}

static void __exit mydev_exit(void)
{
	my_bus_unregister_device(&my_device);
}

module_init(mydev_init);
module_exit(mydev_exit);
MODULE_LICENSE("GPL");
