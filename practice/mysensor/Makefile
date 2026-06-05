obj-m += mysensor.o
KERNDIR=/lib/modules/$(shell uname -r)/build
all:
	make M=$(PWD) -C $(KERNDIR) modules
clean:
	make M=$(PWD) -C $(KERNDIR) clean
