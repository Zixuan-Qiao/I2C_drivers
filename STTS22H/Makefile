KERN_DIR := /usr/src/linux-headers-$(shell uname -r)/
PWD := $(shell pwd)

obj-m := STTS22H.o

all:
	make -C $(KERN_DIR) M=$(PWD) modules

clean:
	make -C $(KERN_DIR) M=$(PWD) clean
