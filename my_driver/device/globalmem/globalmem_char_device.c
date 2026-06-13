
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define GLOBALMEM_MAGIC 'g'
#define MEM_CLEAR _IO(GLOBALMEM_MAGIC, 0)

#define GLOBALMEM_SIZE 0x1000
#define GLOBALMEM_MAJOR 230

#define GLOBALMEM_CDEV_NUM  1

typedef struct {
    struct cdev cdev;
    unsigned char mem[GLOBALMEM_SIZE];
    struct mutex mutex;
}globalmem_cdev;

/*********************************************************
 * 
 *                      模块参数
 * 
 **********************************************************/
static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);

static int globalmem_cdev_num = GLOBALMEM_CDEV_NUM;
module_param(globalmem_cdev_num, int, S_IRUGO);


/*********************************************************
 * 
 *                      全局设备
 * 
 **********************************************************/
globalmem_cdev *globalmem_devp;


/*********************************************************
 * 
 *                      设备操作函数
 * 
 **********************************************************/
static int globalmem_cdev_open(struct inode *inode, struct file *filep);
static int globalmem_cdev_release(struct inode *inode, struct file *filep);
static ssize_t globalmem_cdev_read(struct file *filep, char __user *buf, size_t size, loff_t *ppos);
static ssize_t globalmem_cdev_write(struct file *filep, const char __user *buf, size_t size, loff_t *ppos);
static loff_t globalmem_cdev_llseek(struct file *filep, loff_t offset, int orig);
static long globalmem_cdev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);

struct file_operations g_cdev_fops = {
    .owner = THIS_MODULE,
    .llseek = globalmem_cdev_llseek,
    .read = globalmem_cdev_read,
    .write = globalmem_cdev_write,
    .unlocked_ioctl = globalmem_cdev_ioctl,
    .open = globalmem_cdev_open,
    .release = globalmem_cdev_release
};


/**
 * @brief 打开
 * @param inode: 设备节点
 * @param filep: 文件指针
 * @note 
 */
static int globalmem_cdev_open(struct inode *inode, struct file *filep)
{
    globalmem_cdev *dev = container_of(inode->i_cdev, globalmem_cdev, cdev);
    filep->private_data = dev;
    return 0;
}


/**
 * @brief 释放
 * @param inode: 设备节点
 * @param filep: 文件指针
 * @note 
 */
static int globalmem_cdev_release(struct inode *inode, struct file *filep)
{
    return 0;
}



/**
 * @brief 读取
 * @param filep: 文件指针
 * @param buf: 用户空间内存
 * @param size: 用户空间内存大小
 * @param ppos: 读取位置相对于文件开头的偏移
 * @note 
 */
static ssize_t globalmem_cdev_read(struct file *filep, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long pos = *ppos;
    unsigned int read_count = size;

    int ret = 0;

    globalmem_cdev *dev = filep->private_data;

    if (pos >= GLOBALMEM_SIZE)
    {
        return 0;
    }

    if (read_count > GLOBALMEM_SIZE - pos)
    {
        read_count = (GLOBALMEM_SIZE - pos);
    }

    /*访问共享资源, 增加互斥*/
    mutex_lock(&dev->mutex);
    if (copy_to_user(buf, (dev->mem + pos), read_count))
    {
        ret = -EFAULT;
    }
    else
    {
        *ppos += read_count;
        ret = read_count;
        printk("read %u bytes from %lu\r\n", read_count, pos);
    }
    mutex_unlock(&dev->mutex);

    return ret;
}

/**
 * @brief 写入
 * @param filep: 文件指针
 * @param buf: 用户空间内存
 * @param size: 用户空间内存大小
 * @param ppos: 写入位置相对于文件开头的偏移
 * @note 
 */
static ssize_t globalmem_cdev_write(struct file *filep, const char __user *buf, size_t size, loff_t *ppos)
{
    unsigned long pos = *ppos;
    unsigned int write_count = size;

    int ret = 0;

    globalmem_cdev *dev = filep->private_data;

    if (pos >= GLOBALMEM_SIZE)
    {
        return 0;
    }

    if (write_count > GLOBALMEM_SIZE - pos)
    {
        write_count = (GLOBALMEM_SIZE - pos);
    }

    mutex_lock(&dev->mutex);
    if (copy_from_user((dev->mem + pos), buf, write_count))
    {
        ret = -EFAULT;
    }
    else
    {
        *ppos += write_count;
        ret = write_count;
        printk("write %u bytes to %lu\r\n", write_count, pos);
    }
    mutex_unlock(&dev->mutex);

    return ret;
}


/**
 * @brief seek: 定位文件读写指针
 * @param filep: 文件指针
 * @param offset: 相对偏移
 * @param orig: 偏移参考点, SEEK_SET, SEEK_CUR, SEEK_END
 * @note 
 */
static loff_t globalmem_cdev_llseek(struct file *filep, loff_t offset, int orig)
{
    loff_t ret = 0;
    switch (orig)
    {
        case SEEK_SET:
            if (offset < 0)
            {
                ret = -EINVAL;
                break;
            }
            if ((unsigned int)offset > GLOBALMEM_SIZE)
            {
                ret = -EINVAL;
                break;
            }
            filep->f_pos = (unsigned int)offset;
            ret = filep->f_pos;
            break;
        case SEEK_CUR:
            if ((filep->f_pos + offset) > GLOBALMEM_SIZE)
            {
                ret = -EINVAL;
                break;
            }
            if ((filep->f_pos + offset) < 0)
            {
                ret = -EINVAL;
                break;
            }
            filep->f_pos += offset;
            ret = filep->f_pos;
            break;
        default:
            ret = -EINVAL;
            break;
    }
    return ret;
}


/**
 * @brief ioctl: IO控制
 * @param filep: 文件指针
 * @param cmd: 命令
 * @param arg: 参数指针值
 * @note 
 */
static long globalmem_cdev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    globalmem_cdev *dev = filep->private_data;

    switch (cmd)
    {
        case MEM_CLEAR:
            /*增加互斥访问*/
            mutex_lock(&dev->mutex);
            memset(dev->mem, 0, GLOBALMEM_SIZE);
            mutex_unlock(&dev->mutex);

            printk("dev mem is set to zero\r\n");
            break;
        default:
            return -EINVAL;
    }
    return 0;
}


static void globalmem_setup_cdev(globalmem_cdev *dev, int index)
{
    /*初始化*/
    int ret = 0;

    dev_t devno = MKDEV(globalmem_major, index);
    cdev_init(&dev->cdev, &g_cdev_fops);
    dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&dev->cdev, devno, 1);
    if (ret)
    {
        printk("Error %d adding globalmem%d", ret, index);
    }
}



/*********************************************************
 * 
 *                     编译乱序: 编译器屏障
 * 
 **********************************************************/






/*********************************************************
 * 
 *                     模块初始化
 * 
 **********************************************************/
static int __init globalmem_char_device_init(void)
{
    /*申请设备号 -> 创建cdev -> cdev初始化 -> 添加cdev -> 创建class -> 创建device*/
    int ret = 0;
    int i = 0;
    dev_t devno = MKDEV(globalmem_major, 0);
    
    if (globalmem_major)
    {
        ret = register_chrdev_region(devno, GLOBALMEM_CDEV_NUM, "globalmem");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, GLOBALMEM_CDEV_NUM, "globalmem");
        globalmem_major = MAJOR(devno);
    }
    if (ret < 0)
    {
        return ret;
    }

    globalmem_devp = kzalloc(sizeof(globalmem_cdev) * GLOBALMEM_CDEV_NUM, GFP_KERNEL);
    if (!globalmem_devp)
    {
        ret = -ENOMEM;
        goto fail_malloc;
    }

    for (i = 0; i < GLOBALMEM_CDEV_NUM; ++i)
    {
        mutex_init(&(globalmem_devp + i)->mutex);
        globalmem_setup_cdev(globalmem_devp + i, i);    //创建
    }
    return 0;

fail_malloc:
    unregister_chrdev_region(devno, GLOBALMEM_CDEV_NUM);
    return ret;
}
module_init(globalmem_char_device_init);


/*********************************************************
 * 
 *                     模块卸载
 * 
 **********************************************************/
static void __exit globalmem_char_device_exit(void)
{
    int i = 0;
    for (i = 0; i < GLOBALMEM_CDEV_NUM; ++i)
    {
        cdev_del(&(globalmem_devp + i)->cdev);          //删除
    }
    kfree(globalmem_devp);
    unregister_chrdev_region(MKDEV(globalmem_major, 0), GLOBALMEM_CDEV_NUM);
}
module_exit(globalmem_char_device_exit);

MODULE_AUTHOR("DYWorker001");
MODULE_LICENSE("GPL");


