
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>


#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>

/*****************************************************
 * 
 *                  前置声明
 * 
 ******************************************************/
struct led_config;

/*字符设备操作函数*/
static int led_open(struct inode *inode, struct file *filep);
static int led_release(struct inode *indoe, struct file *filep);
static ssize_t led_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos);
static long led_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

/*驱动操作函数*/
static int led_probe(struct platform_device *pdv);

/*****************************************************
 * 
 *                  宏定义
 * 
 ******************************************************/
/*ioctl命令*/
#define LED_IOC_MAGIC   'l'
#define LED_IOC_ON  _IO(LED_IOC_MAGIC, 0)
#define LED_IOC_OFF _IO(LED_IOC_MAGIC, 1)
#define LED_IOC_SET_BRIGHTNESS _IOW(LED_IOC_MAGIC, 2, int)
#define LED_IOC_SET_CONFIG _IOW(LED_IOC_MAGIC, 3, struct led_config)

/*设备名称*/
#define LED_DEV_NAME "led_node"

/*设备数量*/
#define LED_DEV_NUM 1


static int led_dev_major = 0;
module_param(led_dev_major, int, S_IRUGO);

/*****************************************************
 * 
 *                  资源声明
 * 
 ******************************************************/
struct led_config {
    unsigned int led_id;
    unsigned int brightness;
    unsigned int blink_ms;
};

struct led_device {
    struct cdev cdev;
    struct device_node *node;   //led的node: 硬件资源
    struct device *pdev;

    dev_t devno;                //设备号

    unsigned int __iomem *va_dr;
    unsigned int __iomem *va_ddr;
    unsigned int led_pin;
};
/*设备节点*/
struct device_node *led_test_device_node = NULL;

/*字符设备定义*/
static struct led_device *led_dev = NULL;
static struct class *led_class;
static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .release = led_release,
    .write = led_write,
    .unlocked_ioctl = led_ioctl
};

/*of_device_id, 用于match*/
static const struct of_device_id led_ids[] = {
	{.compatible = "fire,led_test"},
	{/* sentinel */}
};

/*平台设备驱动*/
static struct platform_driver led_drv = {
    .probe = led_probe,
    .driver = {
        .name = "led_drv",
        .owner = THIS_MODULE,
        .of_match_table = led_ids,
    }
};

/*****************************************************
 * 
 *                  字符设备定义
 * 
 ******************************************************/
/**
 * @brief 打开
 * @param inode: 设备节点
 * @param filep: 文件指针
 * @note 把设备数据赋值给文件私有数据，这样文件接口就可以访问它了。由于可能存在文件并发访问，需要考虑互斥
 */
static int led_open(struct inode *inode, struct file *filep)
{
    struct led_device *dev = container_of(inode->i_cdev, struct led_device, cdev);
    filep->private_data = dev;
    return 0;
}

/**
 * @brief 释放
 * @param inode: 设备节点
 * @param filep: 文件指针
 * @note
 */
static int led_release(struct inode *indoe, struct file *filep)
{
    /*open中没有分配资源, 这里不需要释放*/
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
static ssize_t led_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos)
{
    unsigned int input_value = 0;
    unsigned int reg_value=0;

    struct led_device *led_dev = filep->private_data;
    /*获取用户输入的数据, 然后对资源进行操作*/
    if (0 == count)
    {
        return 0;
    }

    if (count > 4)
    {
        count = 4;
    }

    if (copy_from_user(&input_value, buf, count))
    {
        printk("copy from user failed\r\n");
        return -EFAULT;
    }

    /*读取寄存器的内容*/
    reg_value = readl(led_dev->va_dr);

    if (input_value)
    {
        reg_value |= ((unsigned int)0x1 << (led_dev->led_pin+16));
	    reg_value |= ((unsigned int)0X1 << (led_dev->led_pin));
    }
    else
    {
        reg_value |= ((unsigned int)0x1 << (led_dev->led_pin+16));
		reg_value &= ~((unsigned int)0x01 << (led_dev->led_pin));   /*设置GPIO引脚输出低电平*/
    }

    writel(reg_value, led_dev->va_dr);

    return count;
}

/**
 * @brief ioctl: IO控制
 * @param filep: 文件指针
 * @param cmd: 命令
 * @param arg: 参数指针值
 * @note 
 */
static long led_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    unsigned char input_value = 0;
    unsigned int brightness = 0;

    struct led_config config;

    switch (cmd)
    {
        case LED_IOC_ON:
            input_value = 1;
            led_write(filep, &input_value, 1, NULL);
            break;
        case LED_IOC_OFF:
            input_value = 0;
            led_write(filep, &input_value, 1, NULL);
            break;
        case LED_IOC_SET_BRIGHTNESS:
            if (copy_from_user(&brightness, (unsigned int __user*)arg, sizeof(unsigned int)))
            {
                return -EFAULT;
            }
            printk("set led brightness %u\r\n", brightness);
            break;
        case LED_IOC_SET_CONFIG:
            if (copy_from_user(&config, (struct led_config __user*)arg, sizeof(struct led_config)))
            {
                return -EFAULT;
            }
            printk("set led config, blink_ms %u\r\n", config.blink_ms);
            break;
        default:
            break;
    }
    return 0;
}


/*****************************************************
 * 
 *                  驱动定义
 * 
 ******************************************************/
static int led_probe(struct platform_device *pdv)
{
    /*创建字符设备*/
    int ret = -1;
    unsigned int reg_value = 0;
    dev_t devno = 0;

    /******************************************************
     * 
     *          解析设备树，初始化设备硬件资源
     * 
     *******************************************************/
    led_test_device_node = of_find_node_by_path("/led_test");
    if (NULL == led_test_device_node)
    {
        printk("get led_test failed\r\n");
        return -EFAULT;
    }

    led_dev = kzalloc(sizeof(struct led_device), GFP_KERNEL);
    if (NULL == led_dev)
    {
        printk("kzalloc failed\r\n");
        ret = -ENOMEM;
        goto err_out;
    }

    led_dev->node = of_find_node_by_name(led_test_device_node, "led");
    if (NULL == led_dev->node)
    {
        printk("get led node failed\r\n");
        ret = -EFAULT;
        goto err_out;
    }

    /*获取reg属性, 并转化为虚拟地址*/
    led_dev->va_dr = of_iomap(led_dev->node, 0);
    if (NULL == led_dev->va_dr)
    {
        printk("of_iomap failed\r\n");
        ret = -EFAULT;
        goto err_out;
    }

    led_dev->va_ddr = of_iomap(led_dev->node, 1);
    if (NULL == led_dev->va_ddr)
    {
        printk("of_iomap failed\r\n");
        ret = -EFAULT;
        goto err_out;
    }

    /*引脚*/
    led_dev->led_pin = 7;   //这个也可以在设备树中写明

    /*设置寄存器输出模式*/
    reg_value = readl(led_dev->va_ddr);
	reg_value |= ((unsigned int)0x1 << (led_dev->led_pin+16));
	reg_value |= ((unsigned int)0X1 << (led_dev->led_pin));
	writel(reg_value, led_dev->va_ddr);

    /*设置默认值*/
    reg_value = readl(led_dev->va_dr);
	reg_value |= ((unsigned int)0x1 << (led_dev->led_pin+16));
	reg_value |= ((unsigned int)0x1 << (led_dev->led_pin));
    writel(reg_value, led_dev->va_dr);


    /******************************************************
     * 
     *          注册字符设备
     * 
     *******************************************************/
    ret = alloc_chrdev_region(&devno, 0, LED_DEV_NUM, LED_DEV_NAME);
    if (ret < 0)
    {
        printk("alloc chrdev failed\r\n");
        ret = -EFAULT;
        goto err_out;
    }
    led_dev_major = MAJOR(devno);

    led_dev->cdev.owner = THIS_MODULE;
    cdev_init(&led_dev->cdev, &led_fops);
    ret = cdev_add(&led_dev->cdev, devno, LED_DEV_NUM);
    if (ret < 0)
    {
        printk("cdev add failed\r\n");
        goto add_err;
    }

    led_class = class_create(THIS_MODULE, LED_DEV_NAME);
    if (NULL == led_class)
    {
        printk("class create failed\r\n");
        goto add_err;
    }

    led_dev->pdev = device_create(led_class, NULL, devno, NULL, LED_DEV_NAME);
    if (NULL == led_dev->pdev)
    {
        /*销毁led_class*/
        class_destroy(led_class);
        led_class = NULL;
        goto add_err;
    }

    return 0;

add_err:
    unregister_chrdev_region(devno, LED_DEV_NUM);

err_out:
    kfree(led_dev);
    led_dev = NULL;

    return -1;
}


/*****************************************************
 * 
 *                  模块注册和声明
 * 
 ******************************************************/
static int __init led_dts_device_init(void)
{
    int ret = 0;
    ret = platform_driver_register(&led_drv);
    printk("led drv init status %d\r\n", ret);
    return 0;
}
module_init(led_dts_device_init);


static void __exit led_dts_device_exit(void)
{
    /*取消映射*/
    if (led_dev)
    {
        iounmap(led_dev->va_dr);
	    iounmap(led_dev->va_ddr);
    }
    /*销毁字符设备*/
    if (led_class)
    {
        device_destroy(led_class, MKDEV(led_dev_major, 0));
        class_destroy(led_class);
    }

    if (led_dev)
    {
        cdev_del(&led_dev->cdev);
        unregister_chrdev_region(MKDEV(led_dev_major, 0), LED_DEV_NUM);
    }

    /*注销驱动*/
    platform_driver_unregister(&led_drv);

}
module_exit(led_dts_device_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

