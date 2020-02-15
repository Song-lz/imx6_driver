#!/bin/sh
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

obj-m := platform_timer.o
KDIR ?=/home/eurphan/learn/imx6ul/kernel/linux-2.6-imx-rel_imx_4.1.15_1.2.0_ga 
PWD := $(shell pwd)

all:
	make -C $(KDIR) M=$(PWD) modules

clean:
	rm -rf .platform_timer* .tmp* *.o *~core .depend *.cmd *.ko *.mod.c *.order *.symvers
