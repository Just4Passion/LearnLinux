
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>


#define BUFFER_F_SIZE 128

typedef struct
{
    /*设备*/
    const char name[32];
    dev_t devno;
    struct cdev char_dev;
    struct file_operations char_dev_fops;

    struct class *class;
    struct device *device;
    struct device *device2;

    int major;  //主设备号
    int minor;  //次设备号

    const unsigned int dev_cnt;   //设备数量


    /*数据*/
    char buffer_f[BUFFER_F_SIZE];
    char buffer_w[128];
    char buffer_r[128];
}my_char_device;


static int char_device_open(struct inode *inode, struct file *filep);
static int char_device_release(struct inode *inode, struct file *filep);
static ssize_t char_device_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos);
static ssize_t char_device_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos);

my_char_device g_cdevice01 = {
    .name = "MyCharDevice",
    .char_dev_fops = {
        .owner = THIS_MODULE,
        .open = char_device_open,
        .release = char_device_release,
        .write = char_device_write,
        .read = char_device_read,
        .compat_ioctl = NULL,       //32位系统调用ioctl时调用该接口
        .unlocked_ioctl = NULL
    },
    .dev_cnt = 2
};

/*****************************************************
 * 
 *                  打开：open
 * 
 ******************************************************/
static int char_device_open(struct inode *inode, struct file *filep)
{
    printk("char device open: %d\r\n", MINOR(inode->i_rdev));
    switch (MINOR(inode->i_rdev))
    {
        case 0:
            filep->private_data = g_cdevice01.buffer_f;
            break;
        case 1:
            filep->private_data = g_cdevice01.buffer_f;
            break;
    }

    return 0;
}

/*****************************************************
 * 
 *                  释放资源：release
 * 
 ******************************************************/
static int char_device_release(struct inode *inode, struct file *filep)
{
    printk("char device release\r\n");
    return 0;
}

/*****************************************************
 * 
 *                  写入: write
 * 
 ******************************************************/
static ssize_t char_device_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
    unsigned long pos = *ppos;
    size_t write_count = 0;
    char *buffer_w = filep->private_data;

    if (pos > BUFFER_F_SIZE)
    {
        return 0;
    }

    /*超长截断*/
    write_count = (count > (BUFFER_F_SIZE - pos)) ? (BUFFER_F_SIZE - pos) : count;
    ret = copy_from_user(buffer_w, buf, write_count);
    if (0 == ret)
    {
        printk("write data: %s\r\n", buffer_w);
        *ppos += write_count;
        return write_count;
    }
    else
    {
        printk("write failed\r\n");
        return 0;
    }
}


/*****************************************************
 * 
 *                  读取: read. 字符设备的特点是无法随机读取
 * 
 ******************************************************/
static ssize_t char_device_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
    unsigned long pos = *ppos;
    size_t read_count = 0;
    char *buffer_r = filep->private_data;

    if (pos > BUFFER_F_SIZE)
    {
        return 0;
    }

    read_count = (count > (BUFFER_F_SIZE - pos)) ? (BUFFER_F_SIZE - pos) : count;

    memcpy(buffer_r, "I am Char Device\r\n", sizeof("I am Char Device\r\n"));
    read_count = sizeof("I am Char Device\r\n");

    ret = copy_to_user(buf, (buffer_r + pos), read_count);
    if (0 == ret)
    {
        printk("read data\r\n");
        *ppos += read_count;
        return read_count;
    }
    else
    {
        printk("read failed\r\n");
        return 0;
    }
}

/*****************************************************
 * 
 *                  控制: ioctrl
 * 
 ******************************************************/



/*****************************************************
 * 
 *              模块注册和声明
 * 
 ******************************************************/
static int __init char_device_init(void)
{
    /****************************************************
     * 1. 申请设备号
     * 2. 创建cdev并初始化: 设备号, 绑定struct file_operations
     * 3. 注册cdev到系统中
     * 4. 创建class
     * 5. 创建device
     * 
     * 
     ****************************************************/
    int ret = 0;
    printk("===============================char device init==============================\r\n");
    /**********************************
     * int alloc_chrdev_region(dev_t *, unsigned, unsigned, const char *): 申请一个设备号起始值
     *  - 第二个参数表示次设备号的起始值
     ***********************************/
    ret = alloc_chrdev_region(&g_cdevice01.devno, 0, g_cdevice01.dev_cnt, g_cdevice01.name);
    if (ret < 0)
    {
        printk("failed to alloc devno\r\n");
        goto alloc_err;
    }

    g_cdevice01.major = MAJOR(g_cdevice01.devno);
    g_cdevice01.minor = MINOR(g_cdevice01.devno);

    printk("major = %d, minor = %d\r\n", g_cdevice01.major, g_cdevice01.minor);

    /*初始化cdev*/
    g_cdevice01.char_dev.owner = THIS_MODULE;
    cdev_init(&g_cdevice01.char_dev, &g_cdevice01.char_dev_fops);
    ret = cdev_add(&g_cdevice01.char_dev, g_cdevice01.devno, g_cdevice01.dev_cnt);
    if (ret < 0)
    {
        printk("failed to add cdev\r\n");
        goto add_err;
    }

    /********************************
     * 
     * 创建class和device, 并与字符设备绑定：通过设备号进行绑定
     * 
     ********************************/
    g_cdevice01.class = class_create(THIS_MODULE, g_cdevice01.name);
    if (IS_ERR(g_cdevice01.class))
    {
        printk("failed to create class\r\n");
        goto class_err;
    }
    g_cdevice01.device = device_create(g_cdevice01.class, NULL, g_cdevice01.devno, NULL, g_cdevice01.name);
    if (IS_ERR(g_cdevice01.device))
    {
        printk("fail to create device\r\n");
        goto device_err;
    }

    /*第二个设备*/
    g_cdevice01.device2 = device_create(g_cdevice01.class, NULL, g_cdevice01.devno + 1, NULL, "%s01", g_cdevice01.name);
    if (IS_ERR(g_cdevice01.device2))
    {
        printk("fail to create device\r\n");
        goto device_err;
    }

    printk("===========================char device init sucessfully===========================\r\n");
    return 0;

device_err:
    device_destroy(g_cdevice01.class, g_cdevice01.devno + 1);
    device_destroy(g_cdevice01.class, g_cdevice01.devno);
    class_destroy(g_cdevice01.class);

class_err:
    cdev_del(&g_cdevice01.char_dev);

add_err:
    unregister_chrdev_region(g_cdevice01.devno, g_cdevice01.dev_cnt);

alloc_err:
    return ret;
}
module_init(char_device_init);

static void __exit char_device_exit(void)
{
    /****************************************************
     * 1. 先释放统一设备
     * 2. 然后释放class
     * 3. 释放cdev
     * 4. 释放设备号
     * 
     * 
     *****************************************************/
    printk("===============================char device exit==============================\r\n");
    cdev_del(&g_cdevice01.char_dev);
    unregister_chrdev_region(g_cdevice01.devno, g_cdevice01.dev_cnt);
    device_destroy(g_cdevice01.class, g_cdevice01.devno);
    device_destroy(g_cdevice01.class, g_cdevice01.devno + 1);
    class_destroy(g_cdevice01.class);
}
module_exit(char_device_exit);


MODULE_AUTHOR("DYWorker001");
MODULE_LICENSE("GPL");
