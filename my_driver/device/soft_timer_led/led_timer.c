


#include <linux/module.h>
#include <linux/init.h>

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include <linux/spinlock.h>
#include <linux/types.h>


/*********************************************************
 * 
 * 定时器接口
 *      定时器初始化
 *          void timer_setup(struct timer_list *timer, void (*function)(struct timer_list *), unsigned int flags);
 *      定时器启动/重启
 *          int mod_timer(struct timer_list *timer, unsigned long expires); - 设置定时器的到期时间
 *      定时器删除
 *          int del_timer(struct timer_list *timer); - 异步删除指定的定时器
 *          int del_timer_sync(struct timer_list *timer); - 同步删除指定的定时器. 等待回调执行完删除
 *      定时器状态查询
 *          int timer_pending(const struct timer_list *timer); - 查询定时器是否处于活跃状态
 *      from_timer是内核提供的容器of宏, 用于从定时器指针（timer），获取其所在的父结构体指针
 *
 * 系统节拍转换API函数
 *      unsigned long msecs_to_jiffies(const unsigned int msecs); - 毫秒转成jiffies单位(系统节拍单位. 也就是需要多少个系统节拍)
 *      unsigned long usecs_to_jiffies(const unsigned int usecs);
 *      unsigned int jiffies_to_msecs(const unsigned long j); - 节拍转毫秒
 * 
 * 项目简介
 *      通过定时器控制led周期性亮灯
 * 
 */

/*****************************************************
 *                  宏定义
 ******************************************************/
#define LED_IOC_TIMER_MAGIC 'T'
#define LED_IOC_BLINK   _IO(LED_IOC_TIMER_MAGIC, 0)
#define LED_IOC_SET_INTERVAL _IOW(LED_IOC_TIMER_MAGIC, 1, unsigned long)

#define LED_DEV_NUM 1
#define LED_DEV_NAME "led_timer"

/*****************************************************
 *                  类型定义
 ******************************************************/
enum
{
    LED_OFF,
    LED_ON,
    LED_BLINK
};
struct led_chrdev {
    struct cdev cdev;
    struct device *pdev;
    dev_t devno;
    
    unsigned int __iomem *va_dr;    //IO映射
    unsigned int __iomem *va_ddr;
    
    unsigned int hl_pos;    //引脚高低位
    unsigned int led_pin;   //引脚偏移量

    /*设备树节点*/
    struct device_node *device_node;
    
    /*自旋锁*/
    spinlock_t spinlock;
    /*定时器*/
    struct timer_list timer;
    /*LED状态*/
    int led_state;      //0, 关灯; 1, 常亮; 2, 闪烁
    /*定时器间隔时间*/
    unsigned long timer_interval;
};
 /*****************************************************
 *                  函数声明
 ******************************************************/
static int pdrv_led_probe(struct platform_device *pdev);
static int pdrv_led_remove(struct platform_device *pdev);


static int led_open(struct inode *inode, struct file *filep);
static int led_release(struct inode *inode, struct file *filep);
static ssize_t led_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos);
static long led_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

/*****************************************************
 *                  全局变量
 ******************************************************/

static int led_pdev_major = 0;

static const struct of_device_id led_ids[] = {
	{.compatible = "fire,led_test"},
	{/* sentinel */}
};

struct platform_driver led_pdrv = {
    .probe = pdrv_led_probe,
    .remove = pdrv_led_remove,
    .driver = {
        .name = "led_pdrv",
        .of_match_table = led_ids
    }
};

static struct led_chrdev *led_cdev_timer = NULL;
static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .release = led_release,
    .write = led_write,
    .unlocked_ioctl = led_ioctl
};
static struct class *led_class = NULL;


/*****************************************************
 *                  辅助函数
 ******************************************************/
static void led_timer_callback(struct timer_list *ptimer)
{
    /*根据当前LED状态输出反转的LED状态*/
    struct led_chrdev *led_cdev = from_timer(led_cdev, ptimer, timer);
    
    unsigned int reg_value = 0;
    unsigned long flags = 0;

    /*获取自旋锁并保存中断状态*/
    spin_lock_irqsave(&led_cdev->spinlock, flags);

    reg_value = ioread32(led_cdev->va_dr);

    if (LED_BLINK == led_cdev->led_state)
    {
        reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin + 16));   //设置使能位
        reg_value ^= ((unsigned int)0x1 << (led_cdev->led_pin));        //反转电平
        iowrite32(reg_value, led_cdev->va_dr);
    }

    /*重新启动定时器*/
    mod_timer(&led_cdev->timer, jiffies + led_cdev->timer_interval);

    /*释放自旋锁并恢复中断状态*/
    spin_unlock_irqrestore(&led_cdev->spinlock, flags);
}

static int led_parse_dts(struct platform_device *pedv, struct led_chrdev *led_cdev)
{
    /*创建字符设备*/
    unsigned int reg_value = 0;

    struct device_node *led_test_device_node = NULL;

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

    led_cdev->device_node = of_find_node_by_name(led_test_device_node, "led");
    if (NULL == led_cdev->device_node)
    {
        printk("get led node failed\r\n");
        return -EFAULT;
    }

    /*获取reg属性, 并转化为虚拟地址*/
    led_cdev->va_dr = of_iomap(led_cdev->device_node, 0);
    if (NULL == led_cdev->va_dr)
    {
        printk("of_iomap failed\r\n");
        return -EFAULT;
    }

    led_cdev->va_ddr = of_iomap(led_cdev->device_node, 1);
    if (NULL == led_cdev->va_ddr)
    {
        printk("of_iomap failed\r\n");
        return -EFAULT;
    }

    /*引脚*/
    led_cdev->led_pin = 7;   //这个也可以在设备树中写明

    /*设置寄存器输出模式*/
    reg_value = readl(led_cdev->va_ddr);
	reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin+16));
	reg_value |= ((unsigned int)0X1 << (led_cdev->led_pin));
	writel(reg_value, led_cdev->va_ddr);

    /*设置默认值*/
    reg_value = readl(led_cdev->va_dr);
	reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin+16));
	reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin));
    writel(reg_value, led_cdev->va_dr);

    return 0;
}

/*****************************************************
 *                  字符设备操作
 ******************************************************/

static int led_open(struct inode *inode, struct file *filep)
{
    struct led_chrdev *dev = container_of(inode->i_cdev, struct led_chrdev, cdev);
    filep->private_data = dev;
    return 0;
}

static int led_release(struct inode *inode, struct file *filep)
{
    return 0;
}
static ssize_t led_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos)
{
    unsigned int input_value = 0;
    unsigned int reg_value = 0;
    unsigned long flags;    //保存中断状态信息

    struct led_chrdev *led_cdev = filep->private_data;
    if (led_cdev != led_cdev_timer)
    {
        printk("file private data is invalid\r\n");
        return -EFAULT;
    }

    if (count > 4)
    {
        count = 4;
    }

    /*获取输入*/
    if (copy_from_user(&input_value, buf, count))
    {
        return -EFAULT;
    }

    /*获取自旋锁, 并保存中断状态: 保存状态是为了防止在嵌套调用中错误地恢复中断*/
    spin_lock_irqsave(&led_cdev->spinlock, flags);

    /*开灯*/
    if (input_value)
    {
        led_cdev->led_state = 1;
        /*停止定时器*/
        del_timer_sync(&led_cdev->timer);
        /* 读取数据寄存器的值, 修改, 写入 */
        reg_value = readl(led_cdev->va_dr);
        reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin + 16));
        reg_value &= ~((unsigned int)0x01 << (led_cdev->led_pin));   //输出低电平
        writel(reg_value, led_cdev->va_dr);
    }
    /*关灯*/
    {
        led_cdev->led_state = 0;
        /*停止定时器*/
        del_timer_sync(&led_cdev->timer);
        /* 读取数据寄存器的值, 修改, 写入 */
        reg_value = readl(led_cdev->va_dr);
        reg_value |= ((unsigned int)0x1 << (led_cdev->led_pin + 16));
        reg_value |= ((unsigned int)0x01 << (led_cdev->led_pin));     //输出高电平
        writel(reg_value, led_cdev->va_dr);
    }

    /* 释放自旋锁并恢复中断状态 */
    spin_unlock_irqrestore(&led_cdev->spinlock, flags);

    return count;
}

static long led_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct led_chrdev *led_cdev = filep->private_data;
    unsigned long flags = 0;
    unsigned long interval = 0;

    switch (cmd)
    {
        case LED_IOC_BLINK:
            spin_lock_irqsave(&led_cdev->spinlock, flags);
            led_cdev->led_state = LED_BLINK;
            /*开启定时器*/
            mod_timer(&led_cdev->timer, jiffies + led_cdev->timer_interval);
            spin_unlock_irqrestore(&led_cdev->spinlock, flags);
            break;
        case LED_IOC_SET_INTERVAL:
            /*锁, 设置值*/
            if (copy_from_user(&interval, (unsigned long*)arg, sizeof(unsigned long)))
            {
                printk("ioctl copy interval value failed\r\n");
                return -EFAULT;
            }
            spin_lock_irqsave(&led_cdev->spinlock, flags);
            led_cdev->timer_interval = interval;        //这里只设置间隔, 不开启
            spin_unlock_irqrestore(&led_cdev->spinlock, flags);
            break;
        default:
            return -ENOTTY;
    }
    return 0;
}

/*****************************************************
 *                  平台驱动操作
 ******************************************************/
static int pdrv_led_probe(struct platform_device *pdev)
{
    int ret = 0;
    dev_t devno = 0;
    /*完成设备初始化: 硬件资源初始化, 字符设备初始化*/
    /******************************************************
     * 
     *          解析设备树，初始化设备硬件资源
     * 
     *******************************************************/
    led_cdev_timer = devm_kzalloc(&pdev->dev, sizeof(struct led_chrdev), GFP_KERNEL);
    if (!led_cdev_timer) 
    {
        return -ENOMEM;
    }

    /*解析设备树, 获取硬件资源*/
    ret = led_parse_dts(pdev, led_cdev_timer);
    if (ret)
    {
        printk("led_parse_dts failed\r\n");
        return ret;
    }

    /*初始化自旋锁*/
    spin_lock_init(&led_cdev_timer->spinlock);

    /*初始化定时器*/
    timer_setup(&led_cdev_timer->timer, led_timer_callback, 0);
    add_timer(&&led_cdev_timer->timer);
    
    /*设置定时器间隔*/
    led_cdev_timer->timer_interval = HZ/2;

    /*初始灯状态为关灯*/
    led_cdev_timer->led_state = 0;


    /******************************************************
     * 
     *                  注册字符设备
     * 
     *******************************************************/
    ret = alloc_chrdev_region(&devno, 0, LED_DEV_NUM, LED_DEV_NAME);
    if (ret < 0)
    {
        printk("alloc chrdev failed\r\n");
        return -EFAULT;
    }
    led_pdev_major = MAJOR(devno);

    led_cdev_timer->cdev.owner = THIS_MODULE;
    cdev_init(&led_cdev_timer->cdev, &led_fops);
    ret = cdev_add(&led_cdev_timer->cdev, devno, LED_DEV_NUM);
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

    led_cdev_timer->pdev = device_create(led_class, NULL, devno, NULL, LED_DEV_NAME);
    if (NULL == led_cdev_timer->pdev)
    {
        /*销毁led_class*/
        class_destroy(led_class);
        led_class = NULL;
        goto add_err;
    }

    return 0;

add_err:
    unregister_chrdev_region(devno, LED_DEV_NUM);

    return -1;
}

static int pdrv_led_remove(struct platform_device *pdev)
{
    /*删除定时器*/
    del_timer_sync(&led_cdev_timer->timer);   //删除并等待回调完成

    /*取消映射*/
    if (led_cdev_timer)
    {
        if (led_cdev_timer->va_ddr)
        {
            iounmap(led_cdev_timer->va_dr);
        }
        if (led_cdev_timer->va_ddr)
        {
	        iounmap(led_cdev_timer->va_ddr);
        }
    }

    /*销毁统一设备*/
    if (led_class)
    {
        device_destroy(led_class, MKDEV(led_pdev_major, 0));
        class_destroy(led_class);
    }

    /*销毁字符设备*/
    if (led_cdev_timer)
    {
        cdev_del(&led_cdev_timer->cdev);
        unregister_chrdev_region(MKDEV(led_pdev_major, 0), LED_DEV_NUM);
    }

    return 0;
}

/*****************************************************
 *                  模块初始化和退出
 ******************************************************/

static int __init led_timer_init(void)
{
    platform_driver_register(&led_pdrv);
    return 0;
}
module_init(led_timer_init);

static void __exit led_timer_exit(void)
{
    /*注销驱动*/
    platform_driver_unregister(&led_pdrv);
}
module_exit(led_timer_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

