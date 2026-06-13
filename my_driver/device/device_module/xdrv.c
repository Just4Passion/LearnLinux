
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>

extern struct bus_type xbus;


/*********************************************************
 * 
 *                  驱动属性定义
 * 
 **********************************************************/
char *name = "xdrv";

ssize_t drvname_show(struct device_driver *drv, char *bus)
{
    return sprintf(bus, "%s\r\n", name);
}

DRIVER_ATTR_RO(drvname);    //和drvname_show的前缀一致, 都是drvname



/*********************************************************
 * 
 *                  定义一个驱动
 * 
 **********************************************************/
/**
 * @brief 探测: 完成设备初始化
 * @param dev: 设备
 * @note 
 */
int xdrv_probe(struct device *dev)
{
    /*完成硬件的初始化*/
    printk("%s-%s\n", __FILE__, __func__);
	return 0;
}


/**
 * @brief 移除: 释放资源
 * @param dev: 设备
 * @note 
 */
int xdrv_remove(struct device *dev)
{
    printk("%s-%s\n", __FILE__, __func__);
	return 0;
}

static struct device_driver xdrv = {
    .name = "xdev",     //bus通过dev->name和drv->name完成匹配, 所以二者要一致
    .bus = &xbus,
    .probe = xdrv_probe,
    .remove = xdrv_remove
};



/*********************************************************
 * 
 *                  模块注册和声明        
 * 
 **********************************************************/

static int __init xdrv_init(void)
{
    int ret = 0;

    printk("xdrv init\r\n");
    ret = driver_register(&xdrv);
    ret = driver_create_file(&xdrv, &driver_attr_drvname);
    return 0;
}
module_init(xdrv_init);

static void __exit xdrv_exit(void)
{
    printk("xdrv exit\r\n");
    driver_remove_file(&xdrv, &driver_attr_drvname);
    driver_unregister(&xdrv);
}
module_exit(xdrv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

