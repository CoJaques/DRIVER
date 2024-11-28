
### Path to kernel sources ###
KERNELDIR := /home/CoJak/Documents/repo/linux-socfpga/
TOOLCHAIN := /opt/toolchains/arm-linux-gnueabihf_6.4.1/bin/arm-linux-gnueabihf-

# List of object files for the module
playlist_module-y := playlist.o io_manager.o playlist_manager.o irq_manager.o timer_thread_manager.o

# Kernel module target
obj-m := playlist_module.o

PWD := $(shell pwd)
WARN := -W -Wall -Wstrict-prototypes -Wmissing-prototypes

all:
	@echo "Building with kernel sources in $(KERNELDIR)"
	$(MAKE) ARCH=arm CROSS_COMPILE=$(TOOLCHAIN) -C $(KERNELDIR) M=$(PWD) ${WARN}

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod *.mod.c .tmp_versions modules.order Module.symvers *.a

put_music:
	$(TOOLCHAIN)gcc put_music.c -o put_music

deploy:
	@echo "Copying files to export directory"
	cp playlist_module.ko /export/drv/playlist.ko
	cp put_music /export/drv/put_music

install: all put_music deploy clean
