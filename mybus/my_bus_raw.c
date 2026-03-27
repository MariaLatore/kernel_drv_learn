#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#define MY_BUS_DESCR "BPB my bus"

//match devices to drivers; just do a simple name test
static int my_match(struct device *dev, const struct device_driver *driver)
{
	return !strncmp(dev_name(dev), driver->name, strlen(driver->name));
}

//respond to hotplug user events; add environment variable DEV_NAME
static int my_uevent(const struct device *dev, struct kobj_uevent_env *env)
{
	add_uevent_var(env, "DEV_NAME=%s", dev_name(dev));
	return 0;
}

//bus type
struct bus_type my_bus_type = {
	.name = "mybus",
	.match = my_match,
	.uevent = my_uevent,
};

static int __init my_bus_init(void)
{
	return bus_register(&my_bus_type);
}

static void __exit my_bus_exit(void)
{
	bus_unregister(&my_bus_type);
}

module_init(my_bus_init);
module_exit(my_bus_exit);
MODULE_LICENSE("GPL");
