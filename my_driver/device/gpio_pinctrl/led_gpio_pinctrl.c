
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>



/*****************************************************
 * 
 *                  模块简介
 * xxxxxx(模块功能)
 * xxxxxx(基本代码逻辑)
 *
 ******************************************************/

/*****************************************************
 *                  宏定义
 ******************************************************/
#define LED_GPIO_IOC_MAGIC 'g'
#define LED_GPIO_IOC_ON     _IO(LED_GPIO_IOC_MAGIC, 0)
#define LED_GPIO_IOC_OFF     _IO(LED_GPIO_IOC_MAGIC, 1)
#define LED_GPIO_IOC_SET_BRIGHTNESS     _IOW(LED_GPIO_IOC_MAGIC, 2, unsigned int)
#define LED_GPIO_IOC_GET_BRIGHTNESS     _IOR(LED_GPIO_IOC_MAGIC, 3, unsigned int)


#define LED_GPIO_DEV_NAME   "led_gpio"
#define LED_GPIO_DEV_NUM    1

/*****************************************************
 *                  类型定义
 ******************************************************/
struct led_gpio_cdev {
    struct cdev cdev;
    struct device *pdev;

    /*gpio_desc是获取gpio描述的一种方式*/
    struct gpio_desc *pgpio;
    /*还可以通过设备树节点访问函数, 获取gpio信息*/
    int led_pin;
    struct device_node *node;

    unsigned int brightness;
};
 /*****************************************************
 *                  函数声明
 ******************************************************/
/*操作函数*/
static int led_open(struct inode *inode, struct file *filep);
static int led_release(struct inode *inode, struct file *filep);
static ssize_t led_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos);
static long led_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

/*驱动操作函数*/
static int led_probe(struct platform_device *dev);
static int led_remove(struct platform_device *dev);

/*****************************************************
 *                  全局变量
 ******************************************************/
static const struct of_device_id led_ids[] = {
    {.compatible = "fire,led_test"},
    {}
};

static struct platform_driver led_pdrv = {
    .probe = led_probe,
    .remove = led_remove,
    .driver = {
        .name = "led_pdrv",
        .of_match_table = led_ids
    }
};

static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .release = led_release,
    .write = led_write,
    .unlocked_ioctl = led_ioctl
};

static struct class *led_class;

static struct led_gpio_cdev *led_cdev_gpio = NULL;

static int led_gpio_major = 0;


/*****************************************************
 *                  辅助函数
 ******************************************************/


/*****************************************************
 *                  字符设备操作
 ******************************************************/
static int led_open(struct inode *inode, struct file *filep)
{
    struct led_gpio_cdev *dev = container_of(inode->i_cdev, struct led_gpio_cdev, cdev);
    filep->private_data = dev;
    return 0;
}
static int led_release(struct inode *inode, struct file *filep)
{
    return 0;
}
static ssize_t led_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos)
{
    struct led_gpio_cdev *led_cdev = filep->private_data;

    /*获取外部输入*/
    unsigned int input_value = 0;

    if (count > 4)
    {
        count = 4;
    }

    if (copy_from_user(&input_value, buf, count))
    {
        printk("copy from user failed\r\n");
        return -EFAULT;
    }

    /*亮灯*/
    if (input_value)
    {
        gpiod_set_value(led_cdev->pgpio, 0);    //输出低电平, 灯亮
        //gpio_direction_output(led_cdev_gpio->led_pin, 0);
    }
    else
    {
        gpiod_set_value(led_cdev->pgpio, 1);    //输出高电平, 灯灭
        //gpio_direction_output(led_cdev_gpio->led_pin, 1);
    }

    return count;
}
static long led_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct led_gpio_cdev *led_cdev = filep->private_data;

    unsigned int brightness = 0;

    switch (cmd)
    {
        case LED_GPIO_IOC_ON:
            gpiod_set_value(led_cdev->pgpio, 0);    //输出高电平
            break;
        case LED_GPIO_IOC_OFF:
            gpiod_set_value(led_cdev->pgpio, 1);    //输出低电平
            break;
        case LED_GPIO_IOC_SET_BRIGHTNESS:
            if (copy_from_user(&brightness, (unsigned int *)arg, sizeof(unsigned int)))
            {
                dev_err(led_cdev->pdev, "copy from user failed\r\n");
                return -EFAULT;
            }
            led_cdev->brightness = brightness;
            printk("set brightness is %u\r\n", brightness);
            break;
        case LED_GPIO_IOC_GET_BRIGHTNESS:
            brightness = led_cdev->brightness;
            if (copy_to_user((unsigned int *)arg, &brightness, sizeof(unsigned int)))
            {
                dev_err(led_cdev->pdev, "copy from user failed\r\n");
                return -EFAULT;
            }
            printk("get brightness is %u\r\n", brightness);
            break;
        default:
            return -ENOTTY;
    }
    return 0;
}

/*****************************************************
 *                  平台驱动操作
 ******************************************************/
static int led_probe(struct platform_device *pdev)
{
    /*初始化硬件信息, 配置pinctrl, 获取gpio资源*/
    int ret = 0;
    dev_t devno = 0;

    /*这里只有一个节点, 如果是多个灯结果, 需要遍历of_node*/
    led_cdev_gpio = devm_kzalloc(&pdev->dev, sizeof(struct led_gpio_cdev) * LED_GPIO_DEV_NUM, GFP_KERNEL);
    if (IS_ERR(led_cdev_gpio))
    {
        ret = PTR_ERR(led_cdev_gpio);
        dev_err(&pdev->dev, "devm alloc failed, %d\r\n", ret);
        return ret;
    }

    /**********************************************************
     * 
     * 直接从设备节点中获取led的内容: 构造并获取属性gpios或led-gpios的值
     * 
     ***********************************************************/
    led_cdev_gpio->pgpio = gpiod_get(&pdev->dev, "led", GPIOD_OUT_HIGH);
    if (IS_ERR(led_cdev_gpio->pgpio))
    {
        ret = PTR_ERR(led_cdev_gpio->pgpio);
        dev_err(&pdev->dev, "gpiod_get failed, %d\r\n", ret);
        return ret;
    }

    /**********************************************************
     * 
     *              从设备树节点中获取
     * 
     ***********************************************************/
    led_cdev_gpio->node = of_find_node_by_path("/led_test");
    if(IS_ERR(led_cdev_gpio->node))
    {
        ret = PTR_ERR(led_cdev_gpio->pgpio);
        dev_err(&pdev->dev, "failed to get /led_test from dts, %d\r\n", ret);
        return ret;
    }

    led_cdev_gpio->led_pin = of_get_named_gpio(led_cdev_gpio->node, "gpios", 0);
    printk("led gpio pin = %u\r\n", led_cdev_gpio->led_pin);
    /*获取GPIO使用权*/
    ret = gpio_request(led_cdev_gpio->led_pin, "led_gpio");
    /*通过方向控制输出电平*/
    gpio_direction_output(led_cdev_gpio->led_pin, 1);   //输出高电平


    led_cdev_gpio->brightness = 0;

    platform_set_drvdata(pdev, led_cdev_gpio);


    /*字符设备初始化*/
    ret = alloc_chrdev_region(&devno, 0, LED_GPIO_DEV_NUM, LED_GPIO_DEV_NAME);
    if (ret < 0)
    {
        dev_err(&pdev->dev, "failed to alloc chrdev region\n");
        return ret;
    }
    led_gpio_major = devno;

    cdev_init(&led_cdev_gpio->cdev, &led_fops);
    led_cdev_gpio->cdev.owner = THIS_MODULE;

    ret = cdev_add(&led_cdev_gpio->cdev, devno, LED_GPIO_DEV_NUM);
    {
        dev_err(&pdev->dev, "failed to add cdev\n");
        return ret;
    }

    /*创建class*/
    led_class = class_create(THIS_MODULE, LED_GPIO_DEV_NAME);
    if (IS_ERR(led_class))
    {
        ret = PTR_ERR(led_class);
        dev_err(&pdev->dev, "failed to create led class, %d\r\n", ret);
        return -EFAULT;
    }
    led_cdev_gpio->pdev = device_create(led_class, NULL, devno, NULL, LED_GPIO_DEV_NAME);

    return 0;
}
static int led_remove(struct platform_device *dev)
{
    /*释放gpio_desc*/
    gpiod_put(led_cdev_gpio->pgpio);

    /*释放device, class*/
    device_destroy(led_class, led_gpio_major);
    class_destroy(led_class);

    /*释放cdev*/
    cdev_del(&led_cdev_gpio->cdev);
    unregister_chrdev_region(led_gpio_major, LED_GPIO_DEV_NUM);

    return 0;
}
/*****************************************************
 *                  模块初始化和退出
 ******************************************************/
static int __init led_gpio_init(void)
{
    platform_driver_register(&led_pdrv);
    return 0;
}
module_init(led_gpio_init);


static void __exit led_gpio_exit(void)
{
    platform_driver_unregister(&led_pdrv);
}
module_exit(led_gpio_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

