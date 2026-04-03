obj-m += myclass.o
KERN_DIR = /lib/modules/$(shell uname -r)/build
all:
	make M=$(shell pwd) -C $(KERN_DIR)  modules
clean:
	make M=$(shell pwd) -C $(KERN_DIR)  clean
