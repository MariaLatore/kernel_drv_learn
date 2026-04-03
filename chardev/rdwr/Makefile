obj-m += charrw.o
KERNDIR = /lib/modules/$(shell uname -r)/build
all:
	make M=$(shell pwd) -C $(KERNDIR) modules
clean:
	make M=$(shell pwd) -C $(KERNDIR) clean
