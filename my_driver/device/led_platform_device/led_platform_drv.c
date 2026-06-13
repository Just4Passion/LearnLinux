
#include <linux/module.h>
#include <linux/init.h>


#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>

/*****************************************************
 * 
 *                  宏定义
 * 
 ******************************************************/
#define LED_PLATFORM_DEV_MAJOR  243
#define LED_PLATFORM_DEV_NAME   "led"


static int led_pdev_major = LED_PLATFORM_DEV_MAJOR;
module_param(led_pdev_major, int, S_IRUGO);

/*****************************************************
 * 
 *                  类型定义
 * 
 ******************************************************/

struct led_cdev {
    struct cdev cdev;
    struct mutex mutex;
    struct device *dev;

    unsigned int __iomem *va_dr;
    unsigned int __iomem *va_ddr;

    unsigned int led_pin;
};

 /*****************************************************
 * 
 *                  函数声明
 * 
 ******************************************************/
/*操作接口*/
static int led_cdev_open(struct inode *inode, struct file *filep);
static int led_cdev_release(struct inode *inode, struct file *filep);
static ssize_t led_cdev_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos);
/*驱动接口*/
static int led_pdrv_probe(struct platform_device *pdev);
static int led_pdrv_remove(struct platform_device *pdev);

/*****************************************************
 * 
 *                  变量声明
 * 
 ******************************************************/
static struct class *led_class;


/*文件操作接口: 这是设备的操作接口*/
static struct file_operations led_cdev_fops = {
    .open = led_cdev_open,
    .release = led_cdev_release,
    .write = led_cdev_write
};

/*table id, 用于总线进行驱动和设备匹配, 检测到name一致时进行匹配*/
static struct platform_device_id led_pdev_ids[] = {
	{.name = "led_pdev"},
	{}
};
MODULE_DEVICE_TABLE(platform, led_pdev_ids);

/*这是驱动的接口, 负责完成初始化*/
static struct platform_driver led_pdrv = {
	.probe = led_pdrv_probe,
	.remove = led_pdrv_remove,
	.driver.name = "led_pdev",      //driver.name 与 device.driver_override进行匹配
	.id_table = led_pdev_ids,
};

/*****************************************************
 * 
 *                  接口定义
 * 
 ******************************************************/
/**
 * @brief 打开
 * @param inode: 设备节点
 * @param filep: 文件指针
 * @note 把设备数据赋值给文件私有数据，这样文件接口就可以访问它了。由于可能存在文件并发访问，需要考虑互斥
 */
static int led_cdev_open(struct inode *inode, struct file *filep)
{
    unsigned long reg_value = 0;
    struct led_cdev *led_cdev = container_of(inode->i_cdev, struct led_cdev, cdev);
    filep->private_data = led_cdev;

    mutex_lock(&led_cdev->mutex);
    /*设置GPIO模式: 输出*/
    reg_value = ioread32(led_cdev->va_ddr);
	reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin+16));
	reg_value |= ((unsigned int)0X1 << (led_cdev->led_pin));
	iowrite32(reg_value, led_cdev->va_ddr);

    /*设置默认值*/
    reg_value = ioread32(led_cdev->va_dr);
	reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin+16));
	reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin));
	iowrite32(reg_value, led_cdev->va_dr);
    mutex_unlock(&led_cdev->mutex);

    return 0;
}


static int led_cdev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

/**
 * @brief 写入
 * @param filep: 文件指针
 * @param buf: 用户空间内存
 * @param size: 用户空间内存大小
 * @param ppos: 写入位置相对于文件开头的偏移
 * @note 
 */
static ssize_t led_cdev_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
    unsigned long input_value = 0;
    unsigned long reg_value = 0;

    /*首先获取数据*/
    struct led_cdev *led_cdev = filep->private_data;

    /*获取用户写入的值*/
    ret = kstrtoul_from_user(buf, count, 10, &input_value);
    printk("input value = %lu\r\n", input_value);


    mutex_lock(&led_cdev->mutex);
    /*获取寄存器的值*/
    reg_value = ioread32(led_cdev->va_dr);
    printk("reg value = 0x%lx\r\n", reg_value);

    /*开灯*/
    if (input_value)
    {
        reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin+16));
		reg_value |= ((unsigned int)0x01 << (led_cdev->led_pin));    /*设置GPIO引脚输出高电平*/
    }
    /*关灯*/
    else
    {
        reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin+16));
		reg_value &= ~((unsigned int)0x01 << (led_cdev->led_pin));   /*设置GPIO引脚输出低电平*/
    }

    iowrite32(reg_value, led_cdev->va_dr);
    mutex_unlock(&led_cdev->mutex);

    printk("write reg value = 0x%lx\r\n", reg_value);
    ret = count;

    return ret;
}


/*****************************************************
 * 
 *                  驱动定义
 * 
 ******************************************************/
static int led_pdrv_probe(struct platform_device *pdev)
{
    /**************************************
     * 1. probe进行初始化, 首先需要获取硬件信息
     * 2. 对于device，硬件信息一般存放在platform_data中
     * 3. 对于platform_device，可以检查resource内容
     * 
     *************************************/

    int ret = 0;
    dev_t devno = 0;

    struct led_cdev *led_cdev = NULL;

    struct resource *mem_dr = NULL;
    struct resource *mem_ddr = NULL;

    unsigned int *led_hwinfo = NULL;

    /*devm_kzalloc, 绑定到设备生命周期, 设备释放时内存自动释放*/
    led_cdev = devm_kzalloc(&pdev->dev, sizeof(struct led_cdev), GFP_KERNEL);
    if (!led_cdev)
    {
        return -ENOMEM;
    }

    /***************************************
     *          获取设备资源信息
     **************************************/
    led_hwinfo = dev_get_platdata(&pdev->dev);
    mem_dr = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    mem_ddr = platform_get_resource(pdev, IORESOURCE_MEM, 1);

    /***************************************
     *     将设备硬件资源赋值给字符设备
     **************************************/
    led_cdev->led_pin = led_hwinfo[0];
    led_cdev->va_dr = devm_ioremap(&pdev->dev, mem_dr->start, resource_size(mem_dr));
    led_cdev->va_ddr = devm_ioremap(&pdev->dev, mem_ddr->start, resource_size(mem_ddr));

    
    /***************************************
     *             注册字符设备
     * 1. 申请设备号
     * 2. 进行初始化
     * 3. 添加设备
     * 4. 创建class
     * 5. 进行device绑定
     **************************************/
    ret = alloc_chrdev_region(&devno, 0, 1, LED_PLATFORM_DEV_NAME);
    led_pdev_major = MAJOR(devno);
    cdev_init(&led_cdev->cdev, &led_cdev_fops);
    ret = cdev_add(&led_cdev->cdev, devno, 1);
    if (ret < 0)
    {
        printk("failed to add cdev\r\n");
        goto out;
    }

    if (!led_class)
    {
        printk("class is not set\r\n");
        goto out;
    }

    led_cdev->dev = device_create(led_class, NULL, devno, NULL, LED_PLATFORM_DEV_NAME "%d", pdev->id);

    /*初始化互斥体*/
    mutex_init(&led_cdev->mutex);
    
    /*把创建好的字符设备作为驱动数据赋值给device*/
    platform_set_drvdata(pdev, led_cdev);

    return 0;

out:
    unregister_chrdev_region(devno, 1);
    return 0;
}


static int led_pdrv_remove(struct platform_device *pdev)
{
    dev_t devno = MKDEV(led_pdev_major, 0);

    struct led_cdev *led_cdev = platform_get_drvdata(pdev);

    /*********************************************
     *              回收字符设备资源
     * 1. 删除字符设备
     * 2. 销毁绑定的device
     * 3. 回收设备号
     * 
     */
    cdev_del(&led_cdev->cdev);
    device_destroy(led_class, devno);
    unregister_chrdev_region(devno, 1);

    return 0;
}




/*****************************************************
 * 
 *                  模块注册和声明
 * 
 ******************************************************/
static int __init led_pdrv_init(void)
{
    printk("pdrv init\r\n");
    led_class = class_create(THIS_MODULE, "led_pdev");
    if (!led_class)
    {
        printk("class create failed\r\n");
        return -ENOMEM;
    }
    platform_driver_register(&led_pdrv);
    return 0;
}
module_init(led_pdrv_init);

static void __exit led_pdrv_exit(void)
{
    printk("pdrv exit\r\n");\
    platform_driver_unregister(&led_pdrv);
    class_destroy(led_class);
}
module_exit(led_pdrv_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");


