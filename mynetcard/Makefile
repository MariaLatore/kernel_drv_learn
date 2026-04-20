obj-m:= my_ethernet_driver.o
KERNDIR:=/lib/modules/$(shell uname -r)/build
all:
	make -C $(KERNDIR) M=$(shell pwd) modules
clean:
	make -C $(KERNDIR) M=$(shell pwd) clean

