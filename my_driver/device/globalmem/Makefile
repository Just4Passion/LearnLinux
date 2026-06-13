

# uname -r: 5.15.189
KVERS=$(shell uname -r)

obj-m += globalmem_char_device.o

EXTRA_CFLAGS=-g -O0

build: kernel_modules

# ls -al /lib/modules/5.15.189
#
#lrwxrwxrwx  1 root root      48 Sep  1  2025 build -> /usr/src/linux-source-5.15.0/linux-source-5.15.0
#drwxr-xr-x 14 root root    4096 Sep  1  2025 kernel
#-rw-r--r--  1 root root 1542185 Sep  3  2025 modules.alias
#-rw-r--r--  1 root root 1511473 Sep  3  2025 modules.alias.bin
#-rw-r--r--  1 root root   10466 Sep  1  2025 modules.builtin
#-rw-r--r--  1 root root   26284 Sep  3  2025 modules.builtin.alias.bin
#-rw-r--r--  1 root root   12907 Sep  3  2025 modules.builtin.bin
#-rw-r--r--  1 root root   82069 Sep  1  2025 modules.builtin.modinfo
#-rw-r--r--  1 root root  708425 Sep  3  2025 modules.dep
#-rw-r--r--  1 root root  970735 Sep  3  2025 modules.dep.bin
#-rw-r--r--  1 root root     314 Sep  3  2025 modules.devname
#-rw-r--r--  1 root root  242302 Sep  1  2025 modules.order
#-rw-r--r--  1 root root    1596 Sep  3  2025 modules.softdep
#-rw-r--r--  1 root root  637193 Sep  3  2025 modules.symbols
#-rw-r--r--  1 root root  774814 Sep  3  2025 modules.symbols.bin
#lrwxrwxrwx  1 root root      48 Sep  1  2025 source -> /usr/src/linux-source-5.15.0/linux-source-5.15.0


# build -> /usr/src/linux-source-5.15.0/linux-source-5.15.0
kernel_modules:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) modules
clean:
	make -C /lib/modules/$(KVERS)/build M=$(CURDIR) clean



