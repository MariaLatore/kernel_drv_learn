obj-m := helloworld.o
KERNEL_DIRECTORY := /lib/modules/$(shell uname -r)/build
all:
	make M=$(shell pwd) -C $(KERNEL_DIRECTORY) modules
	modinfo ./helloworld.ko
load:
	insmod ./helloworld.ko
	dmesg
unload:
	rmmod ./helloworld.ko
	dmesg
install:
	make M=$(shell pwd) -C $(KERNEL_DIRECTORY)
modules_intall:
	/sbin/depmod -a
clean:
	make M=$(shell pwd) -C $(KERNEL_DIRECTORY) clean
