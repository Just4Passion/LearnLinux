

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>


#define GPIO0_BASE (0xfdd60000)
#define GPIO0_DR (GPIO0_BASE + 0x0004)
#define GPIO0_DDR (GPIO0_BASE + 0x000C)




/*****************************************************
 * 
 *                  定义平台设备
 * 
 ******************************************************/
/*资源声明*/
static struct resource led_resource[] = {
    [0] = DEFINE_RES_MEM(GPIO0_DR, 4),
    [1] = DEFINE_RES_MEM(GPIO0_DDR, 4)
};

/*平台数据声明: 开发板数据*/
unsigned int led_hwinfo[1] = {7};   //偏移

/*接口声明*/
static void led_release(struct device *dev);


struct platform_device led_pdev = {
    .name = "led_pdev",
    .id = 0,
    .num_resources = ARRAY_SIZE(led_resource),
    .resource = led_resource,
    .dev = {
        .release = led_release,
        .platform_data = led_hwinfo,
    }
};

/*****************************************************
 * 
 *                  设备接口定义
 * 
 ******************************************************/
/**
 * @brief release: 释放. 当设备被卸载时调用
 * @param dev: 设备句柄
 * @note 
 */
static void led_release(struct device *dev)
{

}



/*****************************************************
 * 
 *                  模块注册和声明
 * 
 ******************************************************/
static int __init led_pdev_init(void)
{
    printk("pdev init\r\n");
    /*这里可以通过设备树初始化资源*/
    platform_device_register(&led_pdev);
    return 0;
}
module_init(led_pdev_init);

static void __exit led_pdev_exit(void)
{
    printk("pdev exit\r\n");
    platform_device_unregister(&led_pdev);
}
module_exit(led_pdev_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");
