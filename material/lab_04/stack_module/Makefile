### Put here the path to kernel sources! ###
KERNELDIR :=  /home/CoJak/Documents/repo/linux-socfpga
TOOLCHAIN := /opt/toolchains/arm-linux-gnueabihf_6.4.1/bin/arm-linux-gnueabihf-

obj-m := stack.o

PWD := $(shell pwd)
WARN := -W -Wall -Wstrict-prototypes -Wmissing-prototypes

all: stack stack_test

stack_test: stack_test.c
	@echo "Building userspace test application"
	$(TOOLCHAIN)gcc -o $@ stack_test.c -Wall

stack:
	@echo "Building with kernel sources in $(KERNELDIR)"
	$(MAKE) ARCH=arm CROSS_COMPILE=$(TOOLCHAIN) -C $(KERNELDIR) M=$(PWD) ${WARN}

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions modules.order Module.symvers
	rm stack_test
