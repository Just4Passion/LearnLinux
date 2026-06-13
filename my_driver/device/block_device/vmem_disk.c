
#include <linux/module.h>
#include <linux/init.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <linux/blk-mq.h>
#include <linux/bio.h>
#include <linux/genhd.h>
#include <linux/buffer_head.h>


#include <linux/kdev_t.h>



/*****************************************************
 * 
 *                  模块简介
 * 1. register_blkdev()
 * 2. request_queue: 
 *      - blk_init_queue()
 *      - blk_queue_max_hw_sectors()
 *      - blk_queue_logical_block_size()
 * 3. gendisk
 *      - set_capacity()
 *      - add_disk()
 * xxxxxx(基本代码逻辑)
 *
 ******************************************************/

/*****************************************************
 *                  宏定义
 ******************************************************/
#define VMEM_DISK_MINORS 15


#define NDEVICES 1

#define NSECTORS    1024    //一节有1024个扇区
#define HARDSECT_SIZE 512   //一个扇区512字节

#define KERNEL_SECTOR_SIZE 512

/*****************************************************
 *                  类型定义
 ******************************************************/
struct vmem_disk_dev {
    struct block_device *pbdev;
    struct gendisk *gd;

    unsigned long size;
    unsigned char *data;

    /*请求队列*/
    struct request_queue *queue;

    /*自旋锁*/
    spinlock_t lock;

};

enum {
    VMEMD_NOQUEUE,
    VMEMD_QUEUE,
};

 /*****************************************************
 *                  函数声明
 ******************************************************/
static int vmem_disk_getgeo(struct block_device *bdev, struct hd_geometry *geo);

/*****************************************************
 *                  全局变量
 ******************************************************/
static int vmem_disk_major = 0;


static int request_mode;
/*块设备*/
struct vmem_disk_dev *devices = NULL;

struct block_device_operations vmem_disk_ops = {
    .getgeo = vmem_disk_getgeo
};

/*****************************************************
 *                  辅助函数
 ******************************************************/
static void vmem_disk_transfer(struct vmem_disk_dev *dev, unsigned long sector, unsigned long nsect, char *buffer, int write)
{
    unsigned long offset = sector * KERNEL_SECTOR_SIZE;
    unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;

    if ((offset + nbytes) > dev->size) {
        printk("Beyond-end write (%ld %ld)\r\n", offset, nbytes);
        return;
    }

    if (write)
    {
        memcpy(dev->data + offset, buffer, nbytes);
    }
    else
    {
        memcpy(buffer, dev->data + offset, nbytes);
    }
}

static int vmem_disk_xfer_bio(struct vmem_disk_dev *dev, struct bio *bio)
{
    struct bio_vec bved;
    struct bvec_iter iter;
    sector_t sector = bio->bi_iter.bi_sector;

    /*这里的逻辑是在做什么: 传输数据吗*/
    bio_for_each_segment(bvec, bio, iter) {
        char *buffer = __bio_kmap_atomic(bio, iter);
        vmem_disk_transfer(dev, sector, bio_cur_bytes(bio) >> 9, buffer, bio_data_dir(bio) == WRITE);
        sector += bio_cur_bytes(bio) >> 9;
        __bio_kunmap_atomic(buffer);
    }
    
    return 0;
}


static  void vmem_disk_request(struct request_queue *q)
{
    struct request *req;
    struct bio *bio;

    while ((req = blk_peek_request(q)) != NULL)
    {
        struct vmem_disk_dev *dev = req->rq_disk->private_data;
        if (req->cmd_type != REQ_TYPE_FS)
        {
            printk("skip non-fs request\r\n");
            blk_start_request(req);
            __blk_end_request_all(req, -EIO);
            continue;
        }

        blk_start_request(req);
        __rq_for_each_bio(bio, req)
            vmem_disk_xfer_bio(dev, bio);
        __blk_end_rquest_all(req, 0);
    }
}


static void vmem_disk_make_request(struct request_queue *q, struct bio *bio)
{
    struct vmem_disk_dev *dev = q->queuedata;

    int status = 0;

    status = vmem_disk_xfer_bio(dev, bio);
    bio_endio(bio, status);
}



static void setup_device(struct vmem_disk_dev *dev, int which)
{
    memset(dev, 0, sizeof(struct vmem_disk_dev));

    dev->size = NSECTORS * HARDSECT_SIZE;
    dev->data = vmalloc(dev->size);
    if (dev->data == NULL)
    {
        printk("vmalloc failure\r\n");
        return;
    }

    spin_lock_init(&dev->lock);

    switch (request_mode)
    {
        case VMEMD_NOQUEUE:
            dev->queue = blk_alloc_queue(GFP_KERNEL);
            if (dev->queue == NULL)
            {
                goto out_vfree;
            }
            blk_queue_make_request(dev->queue, vmem_disk_make_request);
            break;
        case VMEMD_QUEUE:
            dev->queue = blk_init_queue(vmem_disk_request, &dev->lock);
            if (dev->queue == NULL)
            {
                goto out_vfree;
            }
            break;
        default:
            printk("bad request mode %d, using simple\r\n", request_mode);
            break;
    }

    blk_queue_logical_block_size(dev->queue, HARDSECT_SIZE);
    dev->queue->queuedata = dev;

    dev->gd = alloc_disk(VMEM_DISK_MINORS);
    if (!dev->gd)
    {
        printk("failed to alloc disk\r\n");
        goto out_vfree;
    }

    dev->gd->major = vmem_disk_major;
    dev->gd->first_minor = which * VMEM_DISK_MINORS;    //分区号?
    dev->gd->fops = &vmem_disk_ops;
    dev->gd->queue = dev->queue;
    dev->gd->private_data = dev;

    snprintf(dev->gd->disk_name, 32, "vmem_disk%c", which + 'a');

    set_capacity(dev->gd, NSECTORS * (HARDSECT_SIZE / KERNEL_SECTOR_SIZE));

    add_disk(dev->gd);
    return;

out_vfree:
    if (dev->data)
    {
        vfree(dev->data);
    }
}

/*****************************************************
 *                  块设备操作
 ******************************************************/
static int vmem_disk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
    long size;
    struct vmem_disk_dev *dev = bdev->bd_disk->private_data;

    size = dev->szie * (HARDSECT_SIZE / KERNEL_SECTOR_SIZE);

    geo->cylinders = (size & ~0x3f) >> 6;
    geo->heads = 4;
    geo->sectors = 16;
    geo->start = 4;

    return 0;
}
/*****************************************************
 *                  块设备驱动操作
 ******************************************************/

/*****************************************************
 *                  模块初始化和退出
 ******************************************************/

static int __init vmem_disk_init(void)
{
    int i = 0;

    vmem_disk_major = register_blkdev(vmem_disk_major, "vmem_disk");
    if (vmem_disk_major < 0)
    {
        printk("vmem_disk: failed to get major number\r\n");
        return -EBUSY;
    }

    devices = kmalloc(NDEVICES * sizeof(struct vmem_disk_dev), GFP_KERNEL);
    if (!devices)
    {
        goto out_unregister;
    }
    
    for (i = 0; i < NDEVICES; ++i)
    {
        setup_device(devices + i, i);
    }

    return 0;

out_unregister:
    unregister_blkdev(vmem_disk_major, "sbd");

    return -ENOMEM;
}
module_init(vmem_disk_init);

static void __exit vmem_disk_exit(void)
{
    
}
module_exit(vmem_disk_exit);


