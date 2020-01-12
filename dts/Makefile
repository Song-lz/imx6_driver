#!/bin/sh
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

obj-m := dts.o
KDIR ?=/home/eurphan/learn/imx6ul/kernel/linux-2.6-imx-rel_imx_4.1.15_1.2.0_ga 
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.order *.symvers
