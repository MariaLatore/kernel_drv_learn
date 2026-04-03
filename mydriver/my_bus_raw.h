#ifndef MY_BUS_RAW_H
#define MY_BUS_RAW_H

int my_bus_register_device(struct device *dev);
void my_bus_unregister_device(struct device *dev);

int my_bus_register_driver(struct device_driver *driver);
void my_bus_unregister_driver(struct device_driver *driver);

#endif



