
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>

/*****************************************
 struct bus_type {
	const char		*name;
	const char		*dev_name;
	struct device		*dev_root;
	const struct attribute_group **bus_groups;
	const struct attribute_group **dev_groups;
	const struct attribute_group **drv_groups;

	int (*match)(struct device *dev, struct device_driver *drv);
	int (*uevent)(struct device *dev, struct kobj_uevent_env *env);
	int (*probe)(struct device *dev);
	void (*sync_state)(struct device *dev);
	int (*remove)(struct device *dev);
	void (*shutdown)(struct device *dev);

	int (*online)(struct device *dev);
	int (*offline)(struct device *dev);

	int (*suspend)(struct device *dev, pm_message_t state);
	int (*resume)(struct device *dev);

	int (*num_vf)(struct device *dev);

	int (*dma_configure)(struct device *dev);

	const struct dev_pm_ops *pm;

	const struct iommu_ops *iommu_ops;

	struct subsys_private *p;
	struct lock_class_key lock_key;

	bool need_parent_lock;

	ANDROID_KABI_RESERVE(1);
	ANDROID_KABI_RESERVE(2);
	ANDROID_KABI_RESERVE(3);
	ANDROID_KABI_RESERVE(4);
};

1. match: 当向总线注册一个新的设备或新的驱动时, 会调用该回调函数.
    - 函数负责给驱动匹配合适的设备
    - 函数也负责给设备匹配合适的驱动
2. uevent: 总线上的设备发生添加、移除或其它动作时，就会调用该函数，来通知驱动做出相应的对策
3. probe: 当总线将设备以及驱动相匹配之后, 执行该回调函数
    - 最终会调用驱动提供的probe函数
3. remove: 当设备从总线移除时, 调用该回调函数
4. suspend, resume: 电源管理的相关函数

 */

/************************************************************
 * 
 * 对于一个总线
 *      1. 实现它的match函数
 *      2. 关注它的私有数据
 * 
 * 
 ***********************************************************/




/*内核源码兼容定义*/
#define BUS_ATTR(_name, _mode, _show, _store)   \
    struct bus_attribute bus_attr_##_name = __ATTR(_name, _mode, _show, _store)


/************************************
 * 
 *          总线属性文件
 * 
 *************************************/
static char *bus_name = "xbus";
ssize_t xbus_test_show(struct bus_type *bus, char *buf)
{
    return sprintf(buf, "%s\r\n", bus_name);
}
//BUS_ATTR(xbus_test, S_IRUSR, xbus_test_show, NULL);
BUS_ATTR_RO(xbus_test);


/************************************
 * 
 *          总线match函数定义
 * 
 *************************************/
/**
 * @brief match: 匹配设备和驱动
 * @param dev: 设备
 * @param drv: 驱动
 * @note 
 */
int xbus_match(struct device *dev, struct device_driver *drv)
{
    if (!strncmp(dev_name(dev), drv->name, strlen(drv->name)))
    {
        printk("dev & drv match\r\n");
        return 1;
    }

    return 0;
}

/************************************
 * 
 *          定义一个总线
 * 
 *************************************/
static struct bus_type xbus = {
    .name = "xbus",
    .match = xbus_match
};
EXPORT_SYMBOL(xbus);


/*********************************************************
 * 
 *                  模块注册和声明        
 * bus_register
 * bus_unregister
 * 
 **********************************************************/
static int __init xbus_init(void)
{
    int ret = 0;
    printk("xbus init\r\n");

    /*注册总线*/
    ret = bus_register(&xbus);
    if (ret)
    {
        printk("xbus register failed\r\n");
    }

    /*创建总线属性文件*/
    ret = bus_create_file(&xbus, &bus_attr_xbus_test);

    return 0;
}
module_init(xbus_init);

static void __exit xbus_exit(void)
{
    printk("xbus exit\r\n");

    bus_remove_file(&xbus, &bus_attr_xbus_test);
    bus_unregister(&xbus);
}
module_exit(xbus_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

