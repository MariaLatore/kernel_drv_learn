obj-m += my_bus.o
obj-m += my_bus_raw.o

KDIR :=/lib/modules/$(shell uname -r)/build
all:
	make M=$(shell pwd) -C $(KDIR)  modules
clean:
	make M=$(shell pwd) -C $(KDIR) clean

