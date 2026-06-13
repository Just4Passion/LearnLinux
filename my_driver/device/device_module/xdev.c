

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>


extern struct bus_type xbus;


/*********************************************************
 * 
 *                  设备属性定义
 * 
 **********************************************************/
unsigned long id = 0;

ssize_t xdev_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%ld\r\n", id);
}

ssize_t xdev_id_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int value = 0;
    value = kstrtoul(buf, 10, &id);
    return count;
}

DEVICE_ATTR(xdev_id, S_IRUSR | S_IWUSR, xdev_id_show, xdev_id_store);


/*********************************************************
 * 
 *                  定义一个设备 
 * 
 **********************************************************/

/**
 * @brief release: 释放
 * @param dev: 设备
 * @note 
 */
void xdev_release(struct device *dev)
{
    printk("%s-%s\n", __FILE__, __func__);
}


static struct device xdev = {
    .init_name = "xdev",
    .bus = &xbus,
    .release = xdev_release
};


/*********************************************************
 * 
 *                  模块注册和声明        
 * 
 **********************************************************/
static int __init xdev_init(void)
{
    int ret = 0;

    printk("xdev init\r\n");

    ret = device_register(&xdev);
    if (ret)
    {
        printk("xdev register failed\r\n");
    }

    device_create_file(&xdev, &dev_attr_xdev_id);

    return 0;
}
module_init(xdev_init);


static void __exit xdev_exit(void)
{
    printk("xdev exit\r\n");
    device_remove_file(&xdev, &dev_attr_xdev_id);
    device_unregister(&xdev);
}
module_exit(xdev_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");


