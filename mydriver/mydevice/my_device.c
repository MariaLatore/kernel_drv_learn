#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include "../my_bus_raw.h"

static struct device my_device;
static struct device_driver my_driver = {
	.owner = THIS_MODULE,
	.name = "bpbdriver",
};

static int __init mydev_init(void)
{
	int err;
	my_device.init_name = "drvmydev";
	err = my_bus_register_device(&my_device);
	if(err < 0)
		pr_err("my bus register device:%s failed\n", my_device.init_name);
	err = my_bus_register_driver(&my_driver);
	if(err)
		pr_err("my_bus_register-driver:%s failed", my_driver.name);
	return err;
}

static void __exit mydev_exit(void)
{
	my_bus_unregister_device(&my_device);
	my_bus_unregister_driver(&my_driver);
}

module_init(mydev_init);
module_exit(mydev_exit);
MODULE_LICENSE("GPL");
