obj-m := import.o
obj-m += export.o
KERNEL_DIRECTORY := /lib/modules/$(shell uname -r)/build
all:
	make M=$(shell pwd) -C $(KERNEL_DIRECTORY) modules
	modinfo ./import.ko
	modinfo ./export.ko
clean:
	make M=$(shell pwd) -C $(KERNEL_DIRECTORY) clean
