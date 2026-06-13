#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x3a907ee7, "module_layout" },
	{ 0xe1c98aa5, "xbus" },
	{ 0xaedbc1f8, "device_unregister" },
	{ 0x18ee8e05, "device_remove_file" },
	{ 0xd8e70e7, "device_create_file" },
	{ 0xe67070eb, "device_register" },
	{ 0x92997ed8, "_printk" },
	{ 0x5c3c7387, "kstrtoull" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x3c3ff9fd, "sprintf" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "xbus");


MODULE_INFO(srcversion, "78B19448B24A8AE9E6CB7A4");
