
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>

#include <linux/vmalloc.h>

#include <linux/fs.h>
#include <linux/mm.h>


/*****************************************************
 * 
 *                  模块简介
 * 相关概念
 *  1. 请求队列
 *  2. 多队列标签
 *  3. 逻辑块
 *  4. 物理块
 * xxxxxx(基本代码逻辑)
 *
 ******************************************************/


/*****************************************************
 *                  宏定义
 ******************************************************/
#define RAM_DISK_NAME "ramdisk"
#define RAM_DISK_SECTOR_SIZE 512
#define RAM_DISK_SECTORS (256 * 1024 * 1024 / RAM_DISK_SECTOR_SIZE)
#define RAM_DISK_SIZE (RAM_DISK_SECTORS * RAM_DISK_SECTOR_SIZE)


/*****************************************************
 *                  类型定义
 ******************************************************/
struct ram_disk_dev {
    unsigned char *ram_storage;

    struct gendisk *gd;             /*通用磁盘结构*/
    struct blk_mq_tag_set tag_set;  /*多队列标签集*/
    struct request_queue *queue;    /*请求队列*/

    spinlock_t lock;                /*自旋锁*/
};

/*****************************************************
 *                  函数声明
 ******************************************************/
static blk_status_t ram_disk_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd);

static int ram_disk_open(struct block_device *bdev, fmode_t mode);
static void ram_disk_release(struct gendisk *gd, fmode_t mode);
static int ram_disk_getgeo(struct block_device *bdev, struct hd_geometry *geo);


/*****************************************************
 *                  全局变量
 ******************************************************/
static int ram_disk_major = 0;


static struct blk_mq_ops ram_disk_mq_ops = {
    .queue_rq = ram_disk_queue_rq
};

static struct block_device_operations ram_disk_ops = {
    .owner = THIS_MODULE,
    .open = ram_disk_open,
    .release = ram_disk_release,
    .getgeo = ram_disk_getgeo,
};

static struct ram_disk_dev ramdisk;

/*****************************************************
 *                  辅助函数
 ******************************************************/
static blk_status_t ram_disk_queue_rq(struct blk_mq_hw_ctx *hctx, const struct blk_mq_queue_data *bd)
{
    struct request *rq = bd->rq;
    struct bio_vec bvec;
    struct req_iterator iter;

    loff_t pos = blk_rq_pos(rq) * RAM_DISK_SECTOR_SIZE;
    blk_status_t ret = BLK_STS_OK;

    /*检查请求是否超出设备容量*/
    if (blk_rq_pos(rq) + blk_rq_sectors(rq) > RAM_DISK_SECTORS)
    {
        pr_err("ramdisk: request out of range\r\n");
        ret = BLK_STS_IOERR;
        goto done;
    }

    /*处理请求中的每个bio*/
    rq_for_each_segment(bvec, rq, iter)
    {
        unsigned int len = bvec.bv_len;
        void *buf = page_address(bvec.bv_page) + bvec.bv_offset;

        if (rq_data_dir(rq) == READ)
        {
            memcpy(buf, ramdisk.ram_storage + pos, len);
        }
        else
        {
            memcpy(ramdisk.ram_storage + pos, buf, len);
        }

        pos += len;
    }

done:
    blk_mq_end_request(rq, ret);
    return ret;
}

/*****************************************************
 *                  设备操作
 ******************************************************/
static int ram_disk_open(struct block_device *bdev, fmode_t mode)
{
    pr_info("ramdisk: device opened\n");
    return 0;
}
static void ram_disk_release(struct gendisk *gd, fmode_t mode)
{
    pr_info("ramdisk: device released\n");
}
static int ram_disk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
    /*磁盘几何参数
        磁头数
        每磁道扇区数
        柱面数
        起始扇区号
    */
    return 0;
}


/*****************************************************
 *                  平台驱动操作
 ******************************************************/

/*****************************************************
 *                  模块初始化和退出
 ******************************************************/
static int __init ram_disk_init(void)
{
    int ret = 0;

    pr_info("ramdisk: initializing 256MB RAM disk\r\n");

    /*分配内存*/
    ramdisk.ram_storage = vmalloc(RAM_DISK_SIZE);
    if (!ramdisk.ram_storage)
    {
        pr_err("ramdisk: failed to allocate memory\r\n");
        return -ENOMEM;
    }
    memset(ramdisk.ram_storage, 0, RAM_DISK_SIZE);

    /*初始化自旋锁*/
    spin_lock_init(&ramdisk.lock);

    /*注册块设备*/
    ram_disk_major = register_blkdev(ram_disk_major, RAM_DISK_NAME);
    if (ram_disk_major < 0)
    {
        pr_err("ramdisk: unable to get major number\r\n");
        ret = ram_disk_major;
        goto out_free_mem;
    }

    /*设置多队列标签*/
    ramdisk.tag_set.ops = &ram_disk_mq_ops;
    ramdisk.tag_set.nr_hw_queues = 1;
    ramdisk.tag_set.queue_depth = 128;              //可以放多少bio
    ramdisk.tag_set.numa_node = NUMA_NO_NODE;       //含义
    ramdisk.tag_set.cmd_size = 0;
    ramdisk.tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
    ramdisk.tag_set.driver_data = &ramdisk;


    ret = blk_mq_alloc_tag_set(&ramdisk.tag_set);
    if (ret)
    {
        pr_err("ramdisk: failed to allocate tag set\r\n");
        goto out_unregister_blkdev;
    }

    /*创建请求队列*/
    ramdisk.queue = blk_mq_init_queue(&ramdisk.tag_set);
    if (IS_ERR(ramdisk.queue))
    {
        pr_err("ramdisk: failed to create request queue\r\n");
        ret = PTR_ERR(ramdisk.queue);
        goto out_free_tag_set;
    }

    /*设置逻辑块大小*/
    blk_queue_logical_block_size(ramdisk.queue, RAM_DISK_SECTOR_SIZE);
    blk_queue_physical_block_size(ramdisk.queue, RAM_DISK_SECTOR_SIZE);

    /*分配通用磁盘结构*/
    ramdisk.gd = alloc_disk(1);
    if (!ramdisk.gd)
    {
        pr_err("ramdisk: failed to allocate gendisk\r\n");
        ret = -ENOMEM;
        goto out_cleanup_queue;
    }

    /*设置磁盘属性*/
    ramdisk.gd->major = ram_disk_major;
    ramdisk.gd->first_minor = 0;
    ramdisk.gd->minors = 1;
    ramdisk.gd->fops = &ram_disk_ops;
    ramdisk.gd->queue = ramdisk.queue;
    ramdisk.gd->private_data = &ramdisk;

    snprintf(ramdisk.gd->disk_name, DISK_NAME_LEN, RAM_DISK_NAME);
    set_capacity(ramdisk.gd, RAM_DISK_SECTORS);

    /*添加磁盘到系统*/
    add_disk(ramdisk.gd);

    return 0;

out_cleanup_queue:
    blk_cleanup_queue(ramdisk.queue);
out_free_tag_set:
    blk_mq_free_tag_set(&ramdisk.tag_set);
out_unregister_blkdev:
    unregister_blkdev(ram_disk_major, RAM_DISK_NAME);
out_free_mem:
    vfree(ramdisk.ram_storage);

    return ret;
}


static void __exit ram_disk_exit(void)
{
    pr_info("ramdisk: removing device\r\n");

    /*删除磁盘*/
    if (ramdisk.gd)
    {
        del_gendisk(ramdisk.gd);
        put_disk(ramdisk.gd);
    }

    /*清理请求队列*/
    if (ramdisk.queue)
    {
        blk_cleanup_queue(ramdisk.queue);
    }

    /*释放标签集合*/
    blk_mq_free_tag_set(&ramdisk.tag_set);

    /*注销块设备*/
    if (ram_disk_major > 0)
    {
        unregister_blkdev(ram_disk_major, RAM_DISK_NAME);
    }

    /*释放内存*/
    if (ramdisk.ram_storage)
    {
        vfree(ramdisk.ram_storage);
    }
}


module_init(ram_disk_init);
module_exit(ram_disk_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

