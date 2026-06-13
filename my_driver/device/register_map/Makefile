KERNEL_DIR=../../../kernel/

ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-
export  ARCH  CROSS_COMPILE

obj-m := oled_spi_regmap.o
# out =  test_demo

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) modules
	# $(CROSS_COMPILE)gcc -o $(out) test_demo.c

.PHONY:clean
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) clean
	# rm $(out)

