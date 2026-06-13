

KERNEL_DIR=../../../kernel/

ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-

export ARCH CROSS_COMPILE

obj-m := led_gpio_pinctrl.o

kernel_module:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) modules

.PHONY:clean

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) clean

