
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>


/*********************************************************
 * 
 *                      等待队列
 * 1. wait_queue_head_t: 队列
 * 2. init_waitqueue_head(): 初始化
 * 3. DECLARE_WAITQUEUE(name, tsk): 定义等待队列元素
 * 4. add_wait_queue(wait_queu_head_t *q, wait_queue_t *wait): 添加等待队列
 * 5. remove_wait_queue(wait_queu_head_t *q, wait_queue_t *wait): 移除等待队列
 * 6. wait_event(queue, condition): 等待事件
 *      - wait_event_interruptible(queue, condition)
 *      - wait_event_timeout(queue, condition, timemout)
 *      - wait_event_interruptible_timeout(queue, condition, timeout)
 * 7. wake_up(wait_queue_head_t *queue): 唤醒队列 
 *      - wake_up_interruptible(wait_queue_head_t *queue)
 * 8. sleep_on(wait_queue_head_t *q): 在等待队列上睡眠. 把进程挂起直到资源可获得
 *      - interruptible_sleep_on(wait_queue_head_t *q)
 * 
 **********************************************************/



/*********************************************************
 * 
 *                     宏定义
 * 
 **********************************************************/
#define GLOBAL_FIFO_MAGIC 'f'
#define GLOBALFIFO_MEM_CLEAR _IO(GLOBAL_FIFO_MAGIC, 0)

#define GLOBALFIFO_SIZE 0x1000
#define GLOBALFIFO_MAJOR 0

#define GLOBALFIFO_CDEV_NUM  1

/*********************************************************
 * 
 *                     模块参数声明
 * 
 **********************************************************/
static int globalfifo_major = GLOBALFIFO_MAJOR;
module_param(globalfifo_major, int, S_IRUGO);


/*********************************************************
 * 
 *                     类型声明
 * 
 **********************************************************/
struct globalfifo_dev {
    struct cdev cdev;
    unsigned int current_len;
    unsigned char mem[GLOBALFIFO_SIZE];
    struct mutex mutex;
    wait_queue_head_t r_wait;       //
    wait_queue_head_t w_wait;       //

    struct fasync_struct *async_queue;  //有next指针, 属于链表结构
};


struct globalfifo_dev *globalfifo_cdevp;


/*********************************************************
 * 
 *                     操作函数定义
 * 
 **********************************************************/
static int globalfifo_cdev_open(struct inode *inode, struct file *filep);
static int globalfifo_cdev_release(struct inode *inode, struct file *filep);
static ssize_t globalfifo_cdev_read(struct file *filep, char __user *buf, size_t size, loff_t *ppos);
static ssize_t globalfifo_cdev_write(struct file *filep, const char __user *buf, size_t size, loff_t *ppos);
static long globalfifo_cdev_unlocked_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
static unsigned int globalfifo_cdev_poll(struct file *filep, struct poll_table_struct *wait);
static int globalfifo_cdev_fasync(int fd, struct file *filep, int mode);

struct file_operations g_fifo_cdev_fops = {
    .open = globalfifo_cdev_open,
    .release = globalfifo_cdev_release,
    .read = globalfifo_cdev_read,
    .write = globalfifo_cdev_write,
    .poll = globalfifo_cdev_poll,
    .unlocked_ioctl = globalfifo_cdev_unlocked_ioctl,
    .fasync = globalfifo_cdev_fasync
};

/**
 * @brief 打开
 * @param inode: 设备节点
 * @param filep: 文件指针
 * @note 
 */
static int globalfifo_cdev_open(struct inode *inode, struct file *filep)
{
    /*我需要获取节点对应的cdev, 然后复制给filep->private*/
    struct globalfifo_dev *dev = container_of(inode->i_cdev, struct globalfifo_dev, cdev);
    filep->private_data = dev;
    return 0;
}

/**
 * @brief 释放
 * @param inode: 设备节点
 * @param filep: 文件指针
 * @note 
 */
static int globalfifo_cdev_release(struct inode *inode, struct file *filep)
{
    globalfifo_cdev_fasync(-1, filep, 0);   //将文件从异步通知列表中删除
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
static ssize_t globalfifo_cdev_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;

    /*声明一个等待队列元素 -> 添加到等待队列中 -> 设计等待条件 -> 唤醒后继续执行 -> 拷贝到用户空间 -> 唤醒写任务*/
    struct globalfifo_dev *dev = filep->private_data;

    /*********************************************************
     * #define DECLARE_WAITQUEUE(name, tsk)	struct wait_queue_entry name = __WAITQUEUE_INITIALIZER(name, tsk)
     * 
     * wait_queue_entry是元素; wait_queue_head_t是一个头节点
     * 
     * 在 include/asm-generic/current.h 中（简化版）: #define current get_current()
     * current用于获取当前CPU上正在运行的进程的task_struct
     * 
     * 先声明一个等待队列元素，并告诉等待队列, 这个元素属于哪个进程: current
     *********************************************************/
    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);                //在这里上锁
    add_wait_queue(&dev->r_wait, &wait);    //添加到读等待队列中

    while (dev->current_len == 0)
    {
        if (filep->f_flags & O_NONBLOCK)
        {
            ret = -EAGAIN;
            goto out;
        }
        /*检测到是非阻塞, 设置当前进程状态为TASK_INTERRUPTIBLE*/
        __set_current_state(TASK_INTERRUPTIBLE);        //已经持有锁, 锁本身提供屏障
        mutex_unlock(&dev->mutex);  //进入调度前释放锁, 这样其他进程获取获取锁
        
        /*进行调度: 进程在这里睡眠*/
        schedule(); //触发调度

        /*被唤醒后继续执行*/
        /*检测到信号返回错误: 让用户空间程序知道操作被信号中断了，而不是正常完成或失败*/
        /*当某些信号发生时, 程序没有继续执行下去的必要了: SIGHUP, SIGINT, SIGTERM, SIGSTOP*/
        if (signal_pending(current))
        {
            ret = -ERESTARTSYS;
            goto out2;
        }

        /*重新获取锁, 继续检查条件*/
        mutex_lock(&dev->mutex);
    }

    if (count > dev->current_len)
    {
        count = dev->current_len;
    }

    if (copy_to_user(buf, dev->mem, count))
    {
        ret = -EFAULT;
        goto out;
    }
    else
    {
        /*数据前移, 模拟唤醒缓冲区*/
        memcpy(dev->mem, (dev->mem + count), dev->current_len - count);
        dev->current_len -= count;
        printk("read %ld bytes, current_len: %d\r\n", count, dev->current_len);

        /*唤醒等待写入的线程*/
        wake_up_interruptible(&dev->w_wait);

        /*通知写者：有空间可写*/
        if (dev->async_queue) 
        {
            kill_fasync(&dev->async_queue, SIGIO, POLL_OUT);
            printk("%s kill SIGIO, writeable\r\n", __func__);
        }

        ret = count;
    }

out:
    mutex_unlock(&dev->mutex);

out2:
    remove_wait_queue(&dev->r_wait, &wait);
    set_current_state(TASK_RUNNING);        //无锁保护, 需要显式屏障: 内存屏障确保在状态改变之前的所有内存操作对其他CPU可见

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
static ssize_t globalfifo_cdev_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;

    struct globalfifo_dev *dev = filep->private_data;

    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);
    add_wait_queue(&dev->w_wait, &wait);

    while (dev->current_len == GLOBALFIFO_SIZE)
    {
        if (filep->f_flags & O_NONBLOCK)
        {
            ret = -EAGAIN;
            goto out;
        }

        __set_current_state(TASK_INTERRUPTIBLE);

        mutex_unlock(&dev->mutex);

        schedule();

        if (signal_pending(current))
        {
            ret = -ERESTARTSYS;
            goto out2;
        }

        mutex_lock(&dev->mutex);
    }

    if (count > (GLOBALFIFO_SIZE - dev->current_len))
    {
        count = GLOBALFIFO_SIZE - dev->current_len;
    }

    if (copy_from_user((dev->mem + dev->current_len), buf, count))
    {
        ret = -EFAULT;
        goto out;
    }
    else
    {
        dev->current_len += count;
        printk("written %ld bytes, current_len: %d\r\n", count, dev->current_len);

        wake_up_interruptible(&dev->r_wait);

        /*通知*/
        if (dev->async_queue)
        {
            kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
            printk("%s kill SIGIO, readable\r\n", __func__);
        }

        ret = count;
    }

out:
    mutex_unlock(&dev->mutex);
out2:
    remove_wait_queue(&dev->w_wait, &wait);
    set_current_state(TASK_RUNNING);

    return ret;
}


/**
 * @brief ioctl: IO控制
 * @param filep: 文件指针
 * @param cmd: 命令
 * @param arg: 参数指针值
 * @note 
 */
static long globalfifo_cdev_unlocked_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct globalfifo_dev *dev = filep->private_data;
    switch (cmd)
    {
        case GLOBALFIFO_MEM_CLEAR:
            mutex_lock(&dev->mutex);
            memset(dev->mem, 0, GLOBALFIFO_SIZE);
            mutex_unlock(&dev->mutex);
            printk("fifo set zero\r\n");
            break;
        default:
            return -EINVAL;
    }
    return 0;
}


/**
 * @brief fasync: 异步通知机制
 * @param fd: 文件. 当文件发生改变时, 通知进程 fcntl(fd, cmd, value)
 * @param filep: 文件指针
 * @param mode: 模式
 * @note fcntl(F_SETFL, FASYNC), 调用驱动的fasync()方法
 */
static int globalfifo_cdev_fasync(int fd, struct file *filep, int mode)
{
    struct globalfifo_dev *dev = filep->private_data;
    return fasync_helper(fd, filep, mode, &dev->async_queue);
}


/**
 * @brief poll
 * @param filep: 文件指针
 * @param wait: 等待唤醒的列表
 * @note 
 */
static unsigned int globalfifo_cdev_poll(struct file *filep, struct poll_table_struct *wait)
{
    /****************************************************
     * select的底层依然是调用各个文件的驱动的poll
     * 
     * poll函数返回设备资源的可获取状态: POLLIN, POLLOUT, POLLPRI, POLLERR, POLLNVAL
     *      获取设备结构体指针
     *      加入读等待队列
     *      加入写等待队列
     *      检查标志数据并返回
     * 
     *****************************************************/
    unsigned int mask = 0;
    struct globalfifo_dev *dev = filep->private_data;

    mutex_lock(&dev->mutex);

    poll_wait(filep, &dev->r_wait, wait);   //当前进程注册到驱动的等待队列中，这样当设备状态改变时，驱动能唤醒正在 select/poll/epoll 中等待的进程
    poll_wait(filep, &dev->w_wait, wait);   //当前进程注册到驱动的等待队列中，这样当设备状态改变时，驱动能唤醒正在 select/poll/epoll 中等待的进程 

    if (dev->current_len != 0)
    {
        mask |= POLLIN | POLLRDNORM;
    }

    if (dev->current_len != GLOBALFIFO_SIZE)
    {
        mask |= POLLOUT | POLLWRNORM;
    }

    mutex_unlock(&dev->mutex);

    return mask;
}


 /*********************************************************
 * 
 *                     模块装载和卸载
 * 
 **********************************************************/
static int globalfifo_cdev_setup(struct globalfifo_dev *dev, int index)
{
    int ret = 0;

    dev_t devno = MKDEV(globalfifo_major, index);
    cdev_init(&(dev->cdev), &g_fifo_cdev_fops);
    dev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&(dev->cdev), devno, 1);
    if (ret)
    {
        printk("cdev_add globalfifo cdev failed\r\n");
        return ret;
    }
    return 0;
}
/**
 * @brief 初始化
 * @note 
 */
static int __init globalfifo_cdev_init(void)
{
    int ret = 0;
    int i = 0;

    dev_t devno = MKDEV(globalfifo_major, 0);

    /*分配设备号*/
    if (globalfifo_major)
    {
        ret = register_chrdev_region(devno, GLOBALFIFO_CDEV_NUM, "globalfifo");
    }
    else
    {
        ret = alloc_chrdev_region(&devno, 0, GLOBALFIFO_CDEV_NUM, "globalfifo");
        globalfifo_major = MAJOR(devno);
    }

    /*初始化设备*/
    globalfifo_cdevp = kzalloc(sizeof(struct globalfifo_dev) * GLOBALFIFO_CDEV_NUM, GFP_KERNEL);
    if (!globalfifo_cdevp)
    {
        ret = -ENOMEM;
        goto fail_malloc;
    }

    for (i = 0; i < GLOBALFIFO_CDEV_NUM; ++i)
    {
        globalfifo_cdev_setup((globalfifo_cdevp + i), i);
        mutex_init(&(globalfifo_cdevp + i)->mutex);
        init_waitqueue_head(&(globalfifo_cdevp + i)->r_wait);
        init_waitqueue_head(&(globalfifo_cdevp + i)->w_wait);
    }

    return 0;

fail_malloc:
    unregister_chrdev_region(devno, GLOBALFIFO_CDEV_NUM);
    return ret;
}
module_init(globalfifo_cdev_init);


static void __exit globalfifo_cdev_exit(void)
{
    int i = 0;
    for (i = 0; i < GLOBALFIFO_CDEV_NUM; ++i)
    {
        cdev_del(&(globalfifo_cdevp + i)->cdev);
    }
    kfree(globalfifo_cdevp);
    unregister_chrdev_region(MKDEV(globalfifo_major, 0), GLOBALFIFO_CDEV_NUM);
}
module_exit(globalfifo_cdev_exit);



MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

