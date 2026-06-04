obj-m += myalarm.o
KERNDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
ifdef test
ccflags-y += -DDRIVER_TEST
endif
all:
	$(MAKE) -C $(KERNDIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNDIR) M=$(PWD) clean
