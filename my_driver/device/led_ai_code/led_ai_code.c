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
#include <linux/mutex.h>
#include <linux/mod_devicetable.h>

/*****************************************************
 *                  宏定义
 ******************************************************/
/*ioctl命令*/
#define LED_IOC_MAGIC           'l'
#define LED_IOC_ON              _IO(LED_IOC_MAGIC, 0)
#define LED_IOC_OFF             _IO(LED_IOC_MAGIC, 1)
#define LED_IOC_SET_BRIGHTNESS  _IOW(LED_IOC_MAGIC, 2, int)
#define LED_IOC_SET_CONFIG      _IOW(LED_IOC_MAGIC, 3, struct led_config)

/*设备名称和数量*/
#define LED_DEV_NAME            "led"
#define LED_DEV_NUM             1
#define LED_CLASS_NAME          "led_class"

/*GPIO寄存器偏移*/
#define GPIO_DR_OFFSET          0x0004
#define GPIO_DDR_OFFSET         0x000C

/*****************************************************
 *                  类型定义
 ******************************************************/
struct led_config {
    unsigned int led_id;
    unsigned int brightness;
    unsigned int blink_ms;
};

struct led_device {
    struct cdev cdev;
    struct mutex mutex;
    struct device *dev;
    dev_t devno;
    
    /* 硬件资源 */
    void __iomem *base_addr;        /* GPIO基地址 */
    void __iomem *va_dr;           /* 数据寄存器 */
    void __iomem *va_ddr;          /* 方向寄存器 */
    unsigned int led_pin;          /* LED引脚编号 */
    unsigned int led_active_low;   /* 是否低电平有效 */
};

/*****************************************************
 *                  函数声明
 ******************************************************/
/* 字符设备操作 */
static int led_open(struct inode *inode, struct file *filep);
static int led_release(struct inode *inode, struct file *filep);
static ssize_t led_write(struct file *filep, const char __user *buf, 
                         size_t count, loff_t *ppos);
static long led_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

/* 平台驱动操作 */
static int led_probe(struct platform_device *pdev);
static int led_remove(struct platform_device *pdev);

/*****************************************************
 *                  全局变量
 ******************************************************/
static struct class *led_class;
static int led_major;

static struct file_operations led_fops = {
    .owner          = THIS_MODULE,
    .open           = led_open,
    .release        = led_release,
    .write          = led_write,
    .unlocked_ioctl = led_ioctl,
};

/* 支持设备树和设备ID两种匹配方式 */
static const struct of_device_id led_of_match[] = {
    { .compatible = "fire,led" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, led_of_match);

static const struct platform_device_id led_id_table[] = {
    { .name = "led_pdev" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(platform, led_id_table);

static struct platform_driver led_driver = {
    .probe      = led_probe,
    .remove     = led_remove,
    .driver     = {
        .name           = "led",
        .owner          = THIS_MODULE,
        .of_match_table = led_of_match,
    },
    .id_table   = led_id_table,
};

/*****************************************************
 *                  辅助函数
 ******************************************************/
/**
 * @brief 设置LED状态
 * @param led_dev: LED设备指针
 * @param on: 1-亮, 0-灭
 */
static inline void led_set_state(struct led_device *led_dev, int on)
{
    u32 reg_val;
    
    reg_val = ioread32(led_dev->va_dr);
    
    /* 先写1清除，再设置值 */
    reg_val |= (1 << (led_dev->led_pin + 16));
    
    if (on ^ led_dev->led_active_low) {
        reg_val |= (1 << led_dev->led_pin);
    } else {
        reg_val &= ~(1 << led_dev->led_pin);
    }
    
    iowrite32(reg_val, led_dev->va_dr);
}

/**
 * @brief 初始化GPIO为输出模式
 * @param led_dev: LED设备指针
 */
static inline void led_gpio_init(struct led_device *led_dev)
{
    u32 reg_val;
    
    /* 配置为输出模式 */
    reg_val = ioread32(led_dev->va_ddr);
    reg_val |= (1 << (led_dev->led_pin + 16));  /* 先写1清除 */
    reg_val |= (1 << led_dev->led_pin);         /* 设置为输出 */
    iowrite32(reg_val, led_dev->va_ddr);
    
    /* 默认关闭LED */
    led_set_state(led_dev, 0);
}

/**
 * @brief 从设备树解析硬件资源
 * @param pdev: 平台设备指针
 * @param led_dev: LED设备指针
 * @return 0-成功, 负数-失败
 */
static int led_parse_dt(struct platform_device *pdev, struct led_device *led_dev)
{
    struct device_node *np = pdev->dev.of_node;
    struct device_node *led_np;
    struct resource *res;
    u32 pin_value;
    int ret;
    
    /* 查找LED子节点 */
    led_np = of_get_child_by_name(np, "led");
    if (!led_np) {
        /* 如果没有子节点，直接使用当前节点 */
        led_np = np;
    }
    
    /* 获取reg属性 */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "Failed to get memory resource\n");
        return -EINVAL;
    }
    
    /* 映射寄存器 */
    led_dev->base_addr = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(led_dev->base_addr)) {
        return PTR_ERR(led_dev->base_addr);
    }
    
    led_dev->va_dr = led_dev->base_addr + GPIO_DR_OFFSET / 4;
    led_dev->va_ddr = led_dev->base_addr + GPIO_DDR_OFFSET / 4;
    
    /* 获取引脚编号 */
    ret = of_property_read_u32(led_np, "led-pin", &pin_value);
    if (ret) {
        dev_err(&pdev->dev, "Failed to get led-pin property\n");
        return ret;
    }
    led_dev->led_pin = pin_value;
    
    /* 获取有效电平（可选） */
    led_dev->led_active_low = of_property_read_bool(led_np, "led-active-low");
    
    if (led_np != np) {
        of_node_put(led_np);
    }
    
    return 0;
}

/**
 * @brief 从平台数据解析硬件资源
 * @param pdev: 平台设备指针
 * @param led_dev: LED设备指针
 * @return 0-成功, 负数-失败
 */
static int led_parse_platform_data(struct platform_device *pdev, 
                                   struct led_device *led_dev)
{
    struct resource *res_dr, *res_ddr;
    unsigned int *hwinfo;
    
    /* 获取平台数据 */
    hwinfo = dev_get_platdata(&pdev->dev);
    if (!hwinfo) {
        dev_err(&pdev->dev, "No platform data\n");
        return -EINVAL;
    }
    
    /* 获取寄存器资源 */
    res_dr = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    res_ddr = platform_get_resource(pdev, IORESOURCE_MEM, 1);
    
    if (!res_dr || !res_ddr) {
        dev_err(&pdev->dev, "Failed to get memory resources\n");
        return -EINVAL;
    }
    
    /* 映射寄存器 */
    led_dev->va_dr = devm_ioremap_resource(&pdev->dev, res_dr);
    if (IS_ERR(led_dev->va_dr)) {
        return PTR_ERR(led_dev->va_dr);
    }
    
    led_dev->va_ddr = devm_ioremap_resource(&pdev->dev, res_ddr);
    if (IS_ERR(led_dev->va_ddr)) {
        return PTR_ERR(led_dev->va_ddr);
    }
    
    led_dev->led_pin = hwinfo[0];
    led_dev->led_active_low = 0;
    
    return 0;
}

/*****************************************************
 *                  字符设备操作
 ******************************************************/
static int led_open(struct inode *inode, struct file *filep)
{
    struct led_device *led_dev;
    
    led_dev = container_of(inode->i_cdev, struct led_device, cdev);
    filep->private_data = led_dev;
    
    return 0;
}

static int led_release(struct inode *inode, struct file *filep)
{
    return 0;
}

static ssize_t led_write(struct file *filep, const char __user *buf, 
                         size_t count, loff_t *ppos)
{
    struct led_device *led_dev = filep->private_data;
    unsigned int input_value;
    int ret;
    
    if (count == 0) {
        return 0;
    }
    
    /* 使用kstrtouint_from_user更安全 */
    ret = kstrtouint_from_user(buf, count, 10, &input_value);
    if (ret) {
        return ret;
    }
    
    mutex_lock(&led_dev->mutex);
    led_set_state(led_dev, input_value ? 1 : 0);
    mutex_unlock(&led_dev->mutex);
    
    return count;
}

static long led_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct led_device *led_dev = filep->private_data;
    struct led_config config;
    unsigned int brightness;
    
    switch (cmd) {
    case LED_IOC_ON:
        mutex_lock(&led_dev->mutex);
        led_set_state(led_dev, 1);
        mutex_unlock(&led_dev->mutex);
        break;
        
    case LED_IOC_OFF:
        mutex_lock(&led_dev->mutex);
        led_set_state(led_dev, 0);
        mutex_unlock(&led_dev->mutex);
        break;
        
    case LED_IOC_SET_BRIGHTNESS:
        if (copy_from_user(&brightness, (unsigned int __user *)arg, 
                          sizeof(brightness))) {
            return -EFAULT;
        }
        dev_info(led_dev->dev, "Set brightness: %u\n", brightness);
        break;
        
    case LED_IOC_SET_CONFIG:
        if (copy_from_user(&config, (struct led_config __user *)arg, 
                          sizeof(config))) {
            return -EFAULT;
        }
        dev_info(led_dev->dev, "Set config, blink_ms: %u\n", config.blink_ms);
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
    struct led_device *led_dev;
    dev_t devno;
    int ret;
    
    /* 分配设备结构体 */
    led_dev = devm_kzalloc(&pdev->dev, sizeof(*led_dev), GFP_KERNEL);
    if (!led_dev) {
        return -ENOMEM;
    }
    
    /* 解析硬件资源 */
    if (pdev->dev.of_node) {
        ret = led_parse_dt(pdev, led_dev);
    } else {
        ret = led_parse_platform_data(pdev, led_dev);
    }
    
    if (ret) {
        return ret;
    }
    
    /* 初始化互斥锁 */
    mutex_init(&led_dev->mutex);
    
    /* 初始化GPIO */
    led_gpio_init(led_dev);
    
    /* 注册字符设备 */
    ret = alloc_chrdev_region(&devno, 0, LED_DEV_NUM, LED_DEV_NAME);
    if (ret) {
        dev_err(&pdev->dev, "Failed to alloc chrdev region\n");
        return ret;
    }
    
    led_major = MAJOR(devno);
    led_dev->devno = devno;
    
    cdev_init(&led_dev->cdev, &led_fops);
    led_dev->cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&led_dev->cdev, devno, LED_DEV_NUM);
    if (ret) {
        dev_err(&pdev->dev, "Failed to add cdev\n");
        goto err_cdev_add;
    }
    
    /* 创建设备节点 */
    led_dev->dev = device_create(led_class, &pdev->dev, devno, NULL, 
                                LED_DEV_NAME "%d", pdev->id);
    if (IS_ERR(led_dev->dev)) {
        ret = PTR_ERR(led_dev->dev);
        dev_err(&pdev->dev, "Failed to create device\n");
        goto err_device_create;
    }
    
    /* 保存设备指针 */
    platform_set_drvdata(pdev, led_dev);
    
    dev_info(&pdev->dev, "LED driver probed successfully, pin: %u\n", 
             led_dev->led_pin);
    
    return 0;

err_device_create:
    cdev_del(&led_dev->cdev);
err_cdev_add:
    unregister_chrdev_region(devno, LED_DEV_NUM);
    return ret;
}

static int led_remove(struct platform_device *pdev)
{
    struct led_device *led_dev = platform_get_drvdata(pdev);
    
    /* 关闭LED */
    led_set_state(led_dev, 0);
    
    /* 删除设备节点 */
    device_destroy(led_class, led_dev->devno);
    
    /* 删除字符设备 */
    cdev_del(&led_dev->cdev);
    
    /* 释放设备号 */
    unregister_chrdev_region(led_dev->devno, LED_DEV_NUM);
    
    dev_info(&pdev->dev, "LED driver removed\n");
    
    return 0;
}

/*****************************************************
 *                  模块初始化和退出
 ******************************************************/
static int __init led_init(void)
{
    int ret;
    
    /* 创建类 */
    led_class = class_create(THIS_MODULE, LED_CLASS_NAME);
    if (IS_ERR(led_class)) {
        ret = PTR_ERR(led_class);
        pr_err("Failed to create class\n");
        return ret;
    }
    
    /* 注册平台驱动 */
    ret = platform_driver_register(&led_driver);
    if (ret) {
        class_destroy(led_class);
        pr_err("Failed to register platform driver\n");
        return ret;
    }
    
    pr_info("LED platform driver initialized\n");
    return 0;
}
module_init(led_init);

static void __exit led_exit(void)
{
    platform_driver_unregister(&led_driver);
    class_destroy(led_class);
    pr_info("LED platform driver exited\n");
}
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Optimized Version");
MODULE_DESCRIPTION("Optimized LED Platform Driver");
