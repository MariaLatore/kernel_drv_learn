obj-m += mysensor.o
mysensor-objs := src/mysensordrv.o src/fakesensor.o
KERNDIR = /lib/modules/$(shell uname -r)/build
ccflags-y += -I$(src)/inc -I$(src)/src
all:
ifdef test
	make M=$(shell pwd) -C $(KERNDIR) EXTRA_CFLAGS="-DDRIVER_TEST" modules
else
	make M=$(shell pwd) -C $(KERNDIR) modules
endif

clean:
	make M=$(shell pwd) -C $(KERNDIR) clean

format:
	clang-format -i inc/*.h src/*.c src/*.h
