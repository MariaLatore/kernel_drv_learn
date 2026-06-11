obj-m += myblkdev.o
ifdef test
ccflags-y += "-DTESTDRIVER"
endif
all:
	make M=$(PWD) -C /lib/modules/$(shell uname -r)/build modules
clean:
	make M=$(PWD) -C /lib/modules/$(shell uname -r)/build clean
