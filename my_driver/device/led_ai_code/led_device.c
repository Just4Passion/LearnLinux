/* led_device.c - 用于非设备树情况下的设备注册 */
#include <linux/module.h>
#include <linux/platform_device.h>

#define GPIO0_BASE  0xfdd60000

static struct resource led_resources[] = {
    DEFINE_RES_MEM(GPIO0_BASE, 0x1000),  /* 映射整个GPIO块 */
};

static unsigned int led_platform_data[] = {
    7,      /* LED引脚编号 */
};

static void led_device_release(struct device *dev)
{
    /* 无需特殊处理 */
}

static struct platform_device led_device = {
    .name           = "led_pdev",
    .id             = 0,
    .num_resources  = ARRAY_SIZE(led_resources),
    .resource       = led_resources,
    .dev            = {
        .release        = led_device_release,
        .platform_data  = led_platform_data,
    },
};

static int __init led_device_init(void)
{
    return platform_device_register(&led_device);
}
module_init(led_device_init);

static void __exit led_device_exit(void)
{
    platform_device_unregister(&led_device);
}
module_exit(led_device_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LED Platform Device");
