obj-m = kthread_example.o timer_example.o
KVERSION = $(shell uname -r)
KERNELSRC = /lib/modules/$(KVERSION)/build/
all:
	make -C $(KERNELSRC) M=$(PWD) modules
	rm -rf *.o *~ core .depend .*.cmd *.mod.c .tmp_versions modules.order Module.symvers *.mod *.a
clean:
	make -C $(KERNELSRC) M=$(PWD) clean
