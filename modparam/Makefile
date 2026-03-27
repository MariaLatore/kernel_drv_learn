obj-m := param.o
KERNEL_DIRECTORY := /lib/modules/$(shell uname -r)/build
all:
	make M=$(shell pwd) -C $(KERNEL_DIRECTORY) modules

clean:
	make M=$(shell pwd) -C $(KERNEL_DIRECTORY) clean
