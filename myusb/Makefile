obj-m := myusb.o
KERN := /lib/modules/$(shell uname -r)/build
all:
	make M=$(shell pwd) -C $(KERN) modules
clean:
	make M=$(shell pwd) -C $(KERN) clean
