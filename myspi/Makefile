obj-m := myspidriver/myspidrv.o
obj-m += myspidevice/myspidev.o
KERNDIR = /lib/modules/$(shell uname -r)/build
all:
	make -C $(KERNDIR) M=$(shell pwd) modules
clean:
	make -C $(KERNDIR) M=$(shell pwd) clean
