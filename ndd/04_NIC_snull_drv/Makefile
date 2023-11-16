# Linux 2.6 kernel module Makefile
ifeq ($(KERNELRELEASE),)  

KERNELDIR ?= /lib/modules/$(shell uname -r)/build 
PWD := $(shell pwd)  

.PHONY: build clean  

build:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules  

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c *.order *.symvers
else  

$(info Building with KERNELRELEASE = ${KERNELRELEASE}) 
EXTRA_CFLAGS += -DSNULL_DEBUG
obj-m :=    snull.o  

endif
