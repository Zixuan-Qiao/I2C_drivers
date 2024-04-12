#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

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
	{ 0x3726c6aa, "module_layout" },
	{ 0x98171fcf, "device_destroy" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x5abf8d2b, "class_destroy" },
	{ 0x77abe509, "cdev_del" },
	{ 0x8781d48, "device_create" },
	{ 0x83c50091, "cdev_add" },
	{ 0x2f31c9f4, "cdev_init" },
	{ 0xbf451cca, "__class_create" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x2ec30375, "i2c_get_adapter" },
	{ 0x5f754e5a, "memset" },
	{ 0xae353d77, "arm_copy_from_user" },
	{ 0x8f678b07, "__stack_chk_guard" },
	{ 0x86332725, "__stack_chk_fail" },
	{ 0x51a910c0, "arm_copy_to_user" },
	{ 0x8e865d3c, "arm_delay_ops" },
	{ 0x1d37318b, "i2c_transfer_buffer_flags" },
	{ 0xfe2e64c4, "kmem_cache_alloc_trace" },
	{ 0x8335c0f7, "kmalloc_caches" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0xc5850110, "printk" },
	{ 0x37a0cba, "kfree" },
};

MODULE_INFO(depends, "");
