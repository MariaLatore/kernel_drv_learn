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

//export a simple bus attribute
static ssize_t descr_show(const struct bus_type *bus, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MY_BUS_DESCR);
}

static char scratch_pad[256] = "Hello, this is scratch pad!\n";

static ssize_t pad_show(const struct bus_type *bus, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", scratch_pad);
}

static ssize_t pad_store(const struct bus_type *bus, const char *buf, size_t count)
{
	int size = (count>sizeof(scratch_pad)?sizeof(scratch_pad):count);
	return snprintf(scratch_pad, size, "%s", buf);
}

/*
 * define attribute - attribute name is descr;
 * full name is bus_attr_descr;
 * sysfs entry should be /sys/bus/mybus/descr
 */
static BUS_ATTR_RO(descr);
static BUS_ATTR_RW(pad);
//specify attribute - in module init function
static int __init my_bus_init(void)
{
	int err;
	err = bus_register(&my_bus_type);
	if(err)
		return err;
	err = bus_create_file(&my_bus_type, &bus_attr_descr);
	if(err)
		return err;
	return bus_create_file(&my_bus_type, &bus_attr_pad);
}

static void __exit my_bus_exit(void)
{
	bus_remove_file(&my_bus_type, &bus_attr_descr);
	bus_remove_file(&my_bus_type, &bus_attr_pad);
	bus_unregister(&my_bus_type);
}

module_init(my_bus_init);
module_exit(my_bus_exit);
MODULE_LICENSE("GPL");
