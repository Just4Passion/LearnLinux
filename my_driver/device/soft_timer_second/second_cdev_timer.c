


#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>


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

#define SECOND_DEV_MAJOR 248
#define SECOND_DEV_NAME "second"
#define SECOND_DEV_NUM 1

/*****************************************************
 *                  类型定义
 ******************************************************/
struct second_dev {
    struct cdev cdev;
    struct device *pdev;

    atomic_t counter;
    struct timer_list timer;

    /*引用计数*/
    atomic_t open_counter;  //记录打开的次数, 仅在第一次打开时添加定时器
    spinlock_t lock;

    /*无锁互斥*/
    atomic_t timer_initialized;
};

/*****************************************************
 *                  函数声明
 ******************************************************/
static int second_dev_open(struct inode *inode, struct file *filep);
static int second_dev_release(struct inode *inode, struct file *filep);
static ssize_t second_dev_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos);

/*****************************************************
 *                  全局变量
 ******************************************************/
static int second_dev_major = 0;
module_param(second_dev_major, int, S_IRUSR);

static struct file_operations sdev_fops = {
    .owner = THIS_MODULE,
    .open = second_dev_open,
    .release = second_dev_release,
    .read = second_dev_read
};

static struct second_dev *ps_dev = NULL;
static struct class *second_class = NULL;

/*****************************************************
 *                  辅助函数
 ******************************************************/
static void second_timer_handler(struct timer_list *ptimer)
{
    struct second_dev *pdev = from_timer(pdev, ptimer, timer);

    mod_timer(&pdev->timer, jiffies + HZ);
    atomic_inc(&pdev->counter);

    printk("courrent jiffies is %ld\r\n", jiffies);
}

/*****************************************************
 *                  字符设备操作
 ******************************************************/
static int second_dev_open(struct inode *inode, struct file *filep)
{
    struct second_dev *pdev = container_of(inode->i_cdev, struct second_dev, cdev);

    filep->private_data = pdev;


    spin_lock(&pdev->lock);

    if (atomic_inc_return(&pdev->open_counter) == 1)
    {
        timer_setup(&pdev->timer, second_timer_handler, 0);
        pdev->timer.expires = jiffies + HZ;

        add_timer(&pdev->timer);
    }

    spin_unlock(&pdev->lock);

    return 0;
}

static int second_dev_open_v2(struct inode *inode, struct file *filep)
{
    struct second_dev *pdev = container_of(inode->i_cdev, struct second_dev, cdev);
    
    filep->private_data = pdev;
    
    /*使用 cmpxchg 确保只有一个进程执行初始化*/
    if (atomic_cmpxchg(&pdev->timer_initialized, 0, 1) == 0) 
    {
        timer_setup(&pdev->timer, second_timer_handler, 0);
        pdev->timer.expires = jiffies + HZ;
        add_timer(&pdev->timer);
        /*确保初始化完成*/
        smp_mb();
    } else
    {
        /*等待初始化完成*/
        while (!timer_pending(&pdev->timer)) 
        {
            cpu_relax();
        }
    }
    
    atomic_inc(&pdev->open_counter);
    
    return 0;
}


static int second_dev_release(struct inode *inode, struct file *filep)
{
    struct second_dev *pdev = filep->private_data;

    spin_lock(&pdev->lock);

    if (atomic_dec_return(&pdev->open_counter) == 0)
    {
        del_timer_sync(&pdev->timer);
    }

    spin_unlock(&pdev->lock);
    
    return 0;
}

static ssize_t second_dev_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{
    int counter = 0;
    struct second_dev *pdev = filep->private_data;

    counter = atomic_read(&pdev->counter);

    if (put_user(counter, (int*)buf))
    {
        return -EFAULT;
    }
    else
    {
        return sizeof(int);
    }
    return 0;
}

/*****************************************************
 *                  平台驱动操作
 ******************************************************/

/*****************************************************
 *                  模块初始化和退出
 ******************************************************/
static int __init second_dev_init(void)
{
    int ret = 0;
    dev_t devno = 0;

    /**/
    ps_dev = kzalloc(sizeof(struct second_dev), GFP_KERNEL);
    if (IS_ERR(ps_dev))
    {
        ret = PTR_ERR(ps_dev);
        printk("alloc failed\r\n");
        return ret;
    }

    /*创建字符设备*/
    ret = alloc_chrdev_region(&devno, 0, SECOND_DEV_NUM, SECOND_DEV_NAME);
    if (ret < 0)
    {
        printk("alloc cdev no failed\r\n");
        goto alloc_err;
    }
    second_dev_major = devno;

    cdev_init(&ps_dev->cdev, &sdev_fops);
    ps_dev->cdev.owner = THIS_MODULE;

    /*添加字符设备*/
    ret = cdev_add(&ps_dev->cdev, devno, SECOND_DEV_NUM);
    if (ret < 0)
    {
        printk("cdev add failed\r\n");
        goto add_err;
    }

    second_class = class_create(THIS_MODULE, SECOND_DEV_NAME);
    ps_dev->pdev = device_create(second_class, NULL, devno, NULL, SECOND_DEV_NAME);
    return 0;

add_err:
    unregister_chrdev_region(devno, SECOND_DEV_NUM);

alloc_err:
    kfree(ps_dev);
    ps_dev = NULL;

    return 0;
}
module_init(second_dev_init);

static void __exit second_dev_exit(void)
{
    device_destroy(second_class, second_dev_major);
    class_destroy(second_class);
    
    cdev_del(&ps_dev->cdev);
    unregister_chrdev_region(second_dev_major, SECOND_DEV_NUM);

    kfree(ps_dev);
}
module_exit(second_dev_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");
