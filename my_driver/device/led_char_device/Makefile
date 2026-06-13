
KERNEL_DIR=../../../kernel/

ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-
export  ARCH  CROSS_COMPILE

obj-m := led_char_device.o
out = test_demo
# EXTRA_CFLAGS = -g -O0
# $(CROSS_COMPILE)gcc -o $(out) $(out).c
# rm $(out)
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) modules

.PHONY:clean
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) clean
