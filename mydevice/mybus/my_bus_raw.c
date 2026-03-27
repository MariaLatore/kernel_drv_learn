#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include "../my_bus_raw.h"

#define MY_BUS_DESCR "BPB my bus"

struct bus_type my_bus_type;
static struct device my_bus_device;


//match devices to drivers; just do a simple name test
static int my_match(struct device *dev, const struct device_driver *driver)
{
	int rc = strncmp(dev_name(dev), driver->name, strlen(driver->name));
	if(rc == 0)
		pr_warn("dev name and driver name match\n");
	else
		pr_warn("dev name and driver name mismatch\n");
	return !rc;
}

//respond to hotplug user events; add environment variable DEV_NAME
static int my_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEV_NAME=%s", dev_name(dev));
	return 0;
}

static void my_bus_device_release(struct device *dev)
{
	printk(KERN_WARNING "module:%s is released\n", dev->init_name);
}

static void my_child_device_release(struct device *dev)
{
	printk(KERN_WARNING "child module:%s is released\n", dev->init_name);
}

int my_bus_register_device(struct device *dev)
{
	printk(KERN_WARNING "child module %s registered\n", dev->init_name);
	dev->bus = &my_bus_type;
	dev->release = my_child_device_release;
	dev->parent = &my_bus_device;
	dev_set_name(dev, dev->init_name);
	return device_register(dev);
}

void my_bus_unregister_device(struct device *dev)
{
	printk(KERN_WARNING "child module %s unregistered\n", dev->init_name);
	device_unregister(dev);
}

//bus type
struct bus_type my_bus_type = {
	.name = "mybus",
	.match = my_match,
	.uevent = my_uevent,
};

//bus device
static struct device my_bus_device = {
	.init_name = "mybus0",
	.release = my_bus_device_release,
};


static int __init my_bus_init(void)
{
	int rc = 0;
	rc = bus_register(&my_bus_type);
	rc |= device_register(&my_bus_device);
	return rc;
}

static void __exit my_bus_exit(void)
{
	bus_unregister(&my_bus_type);
	device_unregister(&my_bus_device);
}

/* export register/unregister device functions */
EXPORT_SYMBOL(my_bus_register_device);
EXPORT_SYMBOL(my_bus_unregister_device);
module_init(my_bus_init);
module_exit(my_bus_exit);
MODULE_LICENSE("GPL");
