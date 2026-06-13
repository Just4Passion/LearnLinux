
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/slab.h>

/*****************************************************
 * 
 * 物理地址映射到虚拟地址空间, 对虚拟地址空间读写简介访问物理地址空间
 * 
 ******************************************************/


/*****************************************************
 * 
 * GPIO
 *      引脚复用: 基本IO, 复用, 模拟
 *      寄存器  
 *          - 功能设置寄存器
 *          - 引脚输出寄存器
 *      一个GPIO负责32个引脚
 *      2个寄存器, 每个32bit, 每个寄存器负责16个引脚
 *          高16位为使能位
 *          低16位为引脚位
 * 
 ******************************************************/
#define GPIO0_BASE (0xfdd60000)
//每组GPIO,有2个寄存器,对应32个引脚，每个寄存器负责16个引脚；
//一个寄存器32位，其中高16位都是使能位，低16位对应16个引脚，每个引脚占用1比特位
#define GPIO0_DR_L (GPIO0_BASE + 0x0000)
#define GPIO0_DR_H (GPIO0_BASE + 0x0004)
#define GPIO0_DDR_L (GPIO0_BASE + 0x0008)
#define GPIO0_DDR_H (GPIO0_BASE + 0x000C)

/*****************************************************
 * 
 *                  字符宏定义
 * 
 ******************************************************/
#define LED_CDEV_NAME   "led_cdev"
#define LED_CDEV_NUM    1

#define LED_CDEV_MAJOR  0


/*****************************************************
 * 
 *                  模块参数
 * 
 ******************************************************/
static int led_cdev_major = LED_CDEV_MAJOR;
module_param(led_cdev_major, int, S_IRUGO);


/*****************************************************
 * 
 *                  类型定义
 * 
 ******************************************************/
struct led_reg_info {
    unsigned int gpio_base;     //GPIO基地址
    unsigned int data_addr;     //数据寄存器
    unsigned int drt_addr;      //方向寄存器
    unsigned int led_pin;
};


struct led_cdev{
    struct cdev cdev;
    unsigned int __iomem *va_dr;    // 数据寄存器, 设置输出的电压
    unsigned int __iomem *va_ddr;   // 数据方向寄存器, 设置输入或输出

    unsigned int led_pin;       //偏移

    /*需要一个device*/
    struct device *device;
};


/*****************************************************
 * 
 *                  全局变量资源
 * 
 ******************************************************/
static int led_cdev_open(struct inode *inode, struct file *filep);
static int led_cdev_release(struct inode *inode, struct file *filep);
static ssize_t led_cdev_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos);
static long led_cdev_unlock_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

struct file_operations led_cdev_fops = {
    .open = led_cdev_open,
    .release = led_cdev_release,
    .write = led_cdev_write,
    .unlocked_ioctl = led_cdev_unlock_ioctl
};
/*led全局变量接口*/
struct led_cdev *led_cdev;

/*硬件集合*/
struct led_reg_info led_regs[LED_CDEV_NUM] = {
    {.gpio_base = GPIO0_BASE, .data_addr = GPIO0_DR_H, .drt_addr = GPIO0_DDR_H, .led_pin = 7},
};

/*设备类*/
struct class *led_cdev_class = NULL;

/*****************************************************
 * 
 *              操作函数定义
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
    
    return 0;
}


static int led_cdev_release(struct inode *inode, struct file *filep)
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
    unsigned char input_value = 0;
    unsigned long reg_value = 0;

    /*首先获取数据*/
    struct led_cdev *led_cdev = filep->private_data;

    /*获取用户写入的值*/
    if (copy_from_user(&input_value, buf, 1))
    {
        printk("copy from user failed\r\n");
        ret = -EFAULT;
        goto out;
    }

    /*获取寄存器的值*/
    reg_value = ioread32(led_cdev->va_dr);
    printk("reg value = 0x%lx\r\n", reg_value);

    /*开灯*/
    if (input_value != '0')
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
    printk("write reg value = 0x%lx\r\n", reg_value);
    ret = count;
out:

    return ret;
}

/**
 * @brief ioctl: IO控制
 * @param filep: 文件指针
 * @param cmd: 命令
 * @param arg: 参数指针值
 * @note 
 */
static long led_cdev_unlock_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    return 0;
}


/*****************************************************
 * 
 *              功能函数定义
 * 
 ******************************************************/
static void led_char_device_setup(struct led_cdev *dev, int index)
{
    /*创建设备号*/
    int ret = 0;
    dev_t devno = MKDEV(led_cdev_major, index);

    cdev_init(&dev->cdev, &led_cdev_fops);
    dev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&dev->cdev, devno, 1);
    if (ret)
    {
        printk("cdev_add led failed\r\n");
    }
}



/*****************************************************
 * 
 *              模块注册和声明
 * 
 ******************************************************/
static int __init led_char_device_init(void)
{
    /*分配设备号, 初始化设备，添加设备，创建class，绑定device*/
    int ret = 0;
    dev_t devno = MKDEV(led_cdev_major, 0);
    int i = 0;

    if (led_cdev_major)
    {
        ret = register_chrdev_region(devno, LED_CDEV_NUM, LED_CDEV_NAME);
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, LED_CDEV_NUM, LED_CDEV_NAME);
        led_cdev_major = MAJOR(devno);
    }

    
    led_cdev = kzalloc(sizeof(struct led_cdev) * LED_CDEV_NUM, GFP_KERNEL);
    if (!led_cdev)
    {
        printk("kernel alloc failed\r\n");
        ret = -ENOMEM;
        goto fail_alloc;
    }

    led_cdev_class = class_create(THIS_MODULE, LED_CDEV_NAME);
    if (!led_cdev_class)
    {
        printk("led class create failed\r\n");
        ret = -EFAULT;
        goto out;
    }

    for (i = 0; i < LED_CDEV_NUM; ++i)
    {
        /*构建cdev*/
        led_char_device_setup((led_cdev + i), i);
        printk("==================devno = %d, major = %d, minor = %d\r\n", 
            led_cdev[i].cdev.dev, MAJOR(led_cdev[i].cdev.dev), MINOR(led_cdev[i].cdev.dev));

        /*ioremap*/
        (led_cdev + i)->va_dr = ioremap(led_regs[i].data_addr, 4);
        (led_cdev + i)->va_ddr = ioremap(led_regs[i].drt_addr, 4);

        /*创建device*/
        (led_cdev + i)->device = device_create(led_cdev_class, NULL, (led_cdev + i)->cdev.dev, NULL, LED_CDEV_NAME "%d", i);
        if (!(led_cdev + i)->device)
        {
            printk("led device_create failed\r\n");
        }
    }

    return 0;

out:
    kfree(led_cdev);

fail_alloc:
    unregister_chrdev_region(devno, LED_CDEV_NUM);

    return ret;
}
module_init(led_char_device_init);


static void __exit led_char_device_exit(void)
{
    int i = 0;
    for (i = 0; i < LED_CDEV_NUM; ++i)
    {
        /*取消映射*/
        iounmap(led_cdev[i].va_dr);
        iounmap(led_cdev[i].va_ddr);

        /*销毁device*/
        if (led_cdev[i].device)
        {
            device_destroy(led_cdev_class, led_cdev[i].cdev.dev);
        }

        /*删除字符设备*/
        cdev_del(&led_cdev[i].cdev);
    }

    /*释放设备号*/
    unregister_chrdev_region(MKDEV(led_cdev_major, 0), LED_CDEV_NUM);
    /*释放class*/
    class_destroy(led_cdev_class);
}
module_exit(led_char_device_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

