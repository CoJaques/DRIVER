# !!! Change the path below to the location of your kernel !!!
KERNELDIR := /home/reds/linux-socfpga/
TOOLCHAIN := /opt/toolchains/arm-linux-gnueabihf_6.4.1/bin/arm-linux-gnueabihf-

obj-m := reds_adder_v3.o

PWD := $(shell pwd)
WARN := -W -Wall -Wstrict-prototypes -Wmissing-prototypes

all: reds_adder_v3

reds_adder_v3:
	@echo "Building with kernel sources in $(KERNELDIR)"
	$(MAKE) ARCH=arm CROSS_COMPILE=$(TOOLCHAIN) -C $(KERNELDIR) M=$(PWD) ${WARN}
	rm -rf *.o *~ core .depend .*.cmd *.mod.c .tmp_versions modules.order Module.symvers *.mod *.a
	$(TOOLCHAIN)gcc test_v3.c -lpthread -o test_v3

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions modules.order Module.symvers *.mod *.a test_v3
