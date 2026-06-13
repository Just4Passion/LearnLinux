# uname -r: 5.15.189
KVERS=$(shell uname -r)

obj-m += virtual_emmc.o
out = test_demo

# EXTRA_CFLAGS=-g -O0

build: kernel_modules

# build -> /usr/src/linux-source-5.15.0/linux-source-5.15.0
kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules
	gcc -o $(out) $(out).c

clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean
	rm $(out)


