
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/major.h>

#include <linux/kernel.h>

/*块设备定义*/
#include <linux/blkdev.h>
#include <linux/bio.h>

/*初始化RAM磁盘*/
#include <linux/initrd.h>

/*高端内存映射函数*/
#include <linux/highmem.h>

/*互斥*/
#include <linux/mutex.h>

/*文件系统相关操作*/
#include <linux/fs.h>

/*页面缓存*/
#include <linux/pagemap.h>
#include <linux/radix-tree.h>	// 基数树, 用于高效存储页面

/*内核内存分配*/
#include <linux/slab.h>

/*后备设备信息*/
#include <linux/backing-dev.h>

/*用户空间访问函数*/
#include <linux/uaccess.h>

/*debugfs文件调试*/
#include <linux/debugfs.h>


/*****************************************************
 * 
 *                  模块简介
 * 核心思想
 * 		1. 按需分配物理内存页来模拟一个块设备
 * 		2. 不预先占用内存, 等待数据实际写入某个扇区时, 为该扇区分配物理内存
 * 
 * 关键模块
 * 		Page内存管理
 * 			1. Radix Tree: 基数树. 索引快速查找对应的页指针
 * 				- 索引是以PAGE_SIZE为单位的偏移量
 * 				- 记录当前设备总共占用多少物理页
 * 			2. brd_insert_page
 * 				- 向扇区写入数据时, 必须确保扇区对应的物理页村子啊
 * 			3. brd_free_page
 * 				- 当设备被删除时, 批量释放所有数据页
 *		扇区到页面的映射
			1. 将扇区号映射到物理内存页
			2. 页大小4KB，扇区大小512B，则一个页包含8个扇区
		磁盘总容量管理
			1. 固定总容量, 稀疏存储
 *
 ******************************************************/

/*****************************************************
 *                  宏定义
 ******************************************************/
#define FREE_BATCH 16 // 定义每次批量释放页面的数量



/*****************************************************
 *                  类型定义
 ******************************************************/
struct brd_device {
	int brd_number;
	struct gendisk *brd_disk;
	struct list_head brd_list;		// 链表

	spinlock_t brd_lock;
	struct radix_tree_root brd_pages;		// 页面, 内存页基数树
	u64 brd_nr_pages;
};
/*****************************************************
 *                  函数声明
 ******************************************************/
/*块设备操作函数合集*/
static blk_qc_t brd_submit_bio(struct bio *bio);
static int brd_rw_page(struct block_device *bdev, sector_t sector,
	struct page *page, unsigned int op);


/*****************************************************
 *                  全局变量
 ******************************************************/
/*默认设备数量*/
static int rd_nr = CONFIG_BLK_DEV_RAM_COUNT;
module_param(rd_nr, int, 0444);	// 权限为只读
MODULE_PARM_DESC(rd_nr, "Maximum number of brd devices");

/*默认设备大小(KB)*/
unsigned long rd_size = CONFIG_BLK_DEV_RAM_SIZE;
module_param(rd_size, ulong, 0444);		// 权限为只读
MODULE_PARM_DESC(rd_size, "Size of each RAM disk in kbytes.");

/*每个设备的从设备号数量, 默认为1, 不支持分区*/
static int max_part = 1;
module_param(max_part, int, 0444);
MODULE_PARM_DESC(max_part, "Num Minors to reserve between devices");




/*块设备操作函数合集*/
static const struct block_device_operations brd_fops = {
	.owner = THIS_MODULE,
	.submit_bio = brd_submit_bio,	// 提交 bio 的处理函数
	.rw_page = brd_rw_page			// 整页读写函数
};

/*链表, 管理所有brd_device*/
static LIST_HEAD(brd_devices);
/*互斥锁, 保护链表操作*/
static DEFINE_MUTEX(brd_devices_mutex);

/*debugfs目录的dentry指针*/
static struct dentry *brd_debugfs_dir;

/*****************************************************
 *                  辅助函数
 ******************************************************/
/**
 * @brief 查找给定扇区对应的页面
 * @param 
 * @note 
 */
static struct page *brd_lookup_page(struct brd_device *brd, sector_t sector)
{
	pgoff_t idx;
	struct page *page;

	rcu_read_lock();
	idx = sector >> PAGE_SECTORS_SHIFT;
	page = radix_tree_lookup(&brd->brd_pages, idx);
	rcu_read_unlock();

	BUG_ON(page && page->index != idx);

	return page;
}

/**
 * @brief 插入一个页面
 * @param 
 * @note 
 */
static int brd_insert_page(struct brd_device *brd, sector_t sector)
{
	pgoff_t idx;			// 页面索引
	struct page *page; 	// 指向页面的指针
	gfp_t gfp_flags;		// 内存分配标志

	page = brd_lookup_page(brd, sector);	// 先查找页面是否已存在
	if (page)
	{
		return 0;	// 存在则直接返回成功
	}

	/*设置分配标志: 禁止IO, 清零页面, 允许使用高端内存*/
	gfp_flags = GFP_NOIO | __GFP_ZERO | __GFP_HIGHMEM;
	page = alloc_page(gfp_flags);	// 分配一个新的页面
	if (!page)
	{
		return -ENOMEM;
	}

	/*基数树插入需要预加载内存, 避免在持锁时进行可能睡眠的内存分配*/
	if (radix_tree_preload(GFP_NOIO))
	{
		__free_page(page);
		return -ENOMEM;
	}

	spin_lock(&brd->brd_lock);
	idx = sector >> PAGE_SECTORS_SHIFT;	// 计算页面索引
	page->index = idx;
	if (radix_tree_insert(&brd->brd_pages, idx, page))
	{
		__free_page(page);
		page = radix_tree_lookup(&brd->brd_pages, idx);
		BUG_ON(!page);
		BUG_ON(page->index != idx);
	}
	else
	{
		brd->brd_nr_pages++;		// 插入成功, 增加设备页面计数
	}
	spin_unlock(&brd->brd_lock);	// 释放自旋锁

	radix_tree_preload_end();
	return 0;
}


/**
 * @brief 释放所有后备存储页面和基数树
 * @param 
 * @note 
 */
static void brd_free_pages(struct brd_device *brd)
{
	unsigned long pos = 0;		// 起始搜索位置
	struct page *pages[FREE_BATCH];
	int nr_pages;		// 每次查找到的页面数量

	do {
		int i;

		/*批量查找基数树种的页面*/
		nr_pages = radix_tree_gang_lookup(&brd->brd_pages, (void **)pages, pos, FREE_BATCH);

		/*遍历查找到的每一个页面*/
		for (i = 0; i < nr_pages; ++i)
		{
			void *ret;

			BUG_ON(pages[i]->index < pos);
			pos = pages[i]->index;
			ret = radix_tree_delete(&brd->brd_pages, pos);
			BUG_ON(!ret || ret != pages[i]);
			__free_page(pages[i]);
		}

		pos++;	// 移动到下一个索引位置

		/*移除大容量的RAM需要很长时间*/
		cond_resched();		// 主动让出CPU进行调度
	}while(nr_pages == FREE_BATCH);
}

/**
 * @brief 释放所有后备存储页面和基数树
 * @param 
 * @note 
 */
static int copy_to_brd_setup(struct brd_device *brd, sector_t sector, size_t n)
{
	/*计算扇区在内存内的偏移量*/
	unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;

	size_t copy;	// 本次复制需要的字节数
	int ret;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	ret = brd_insert_page(brd, sector);	// 为目标扇区所在的第一个页面分配/查找页面
	if (ret)
	{
		return ret;
	}

	if (copy < n)
	{
		sector += copy >> SECTOR_SHIFT;	// 计算下一个扇区号
		ret = brd_insert_page(brd, sector);
	}
	return ret;
}

/**
 * @brief 从src中复制n个字节到brd设备的指定扇区
 * @param 
 * @note 
 */
static void copy_to_brd(struct brd_device *brd, const void *src, sector_t sector, size_t n)
{
	struct page *page;		// 目标页面
	void *dst;				// 目标页面的内核虚拟地址
	unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT; // 扇区在页面内的偏移
	size_t copy;			// 本次复制的字节数

	copy = min_t(size_t, n, PAGE_SIZE - offset);	// 计算第一个页面需要复制的字节数
	page = brd_lookup_page(brd, sector);
	BUG_ON(!page);

	dst = kmap_atomic(page);			// 原子映射页面到内核地址空间
	/*首先获取页面地址, 地址加上扇区在页面里面的偏移*/
	memcpy(dst + offset, src, copy);	//	执行数据复制
	kunmap_atomic(dst);					// 解除映射

	if (copy < n)
	{
		src += copy;
		sector += copy >> SECTOR_SHIFT; // 更新扇区号
		copy = n - copy;
		page = brd_lookup_page(brd, sector);	// 查找下一个页面
		BUG_ON(!page);

		dst = kmap_atomic(page);
		memcpy(dst + offset, src, copy);
		kunmap_atomic(dst);
	}
}

/**
 * @brief 从brd指定扇区复制n字节到dst
 * @param 
 * @note 
 * 1. kmap_atomic: 临时映射高端内存页到内核地址空间
 * 			- 作用
 * 				- 临时建立映射
 * 				- 支持高端内存
 * 				- 原子行下文安全
 *			- 入参: struct page *page
 * 				- 指向要映射的物理页描述符
 * 			- 返回值
 * 				- 内核虚拟地址指针
 */
static void copy_from_brd(void *dst, struct brd_device *brd, sector_t sector, size_t n)
{
	struct page *page;
	void *src;
	unsigned int offset = (sector & (PAGE_SECTORS-1)) << SECTOR_SHIFT;

	size_t copy;

	copy = min_t(size_t, n, PAGE_SIZE - offset);
	page = brd_lookup_page(brd, sector);
	if (page)
	{
		/*映射, 复制, 解除映射*/
		src = kmap_atomic(page);
		memcpy(dst, src + offset, copy);
		kunmap_atomic(src);
	}
	else
	{
		/*页面不存在, 说明该区域从未被写入, 内容为全0*/
		memset(dst, 0, copy);	
	}

	if (copy < n)
	{
		dst += copy;
		sector += copy >> SECTOR_SHIFT;
		copy = n - copy;
		page = brd_lookup_page(brd, sector);

		if (page)
		{
			src = kmap_atomic(page);
			memcpy(dst, src + offset, copy);
			kunmap_atomic(src);
		}
		else
		{
			memset(dst, 0, copy);
		}
	}
}

/**
 * @brief 处理bio请求中的单个bvec
 * @param 
 * @note 
 */
static int brd_do_bvec(struct brd_device *brd, struct page *page, 
	unsigned int len, unsigned int off, unsigned int op, sector_t sector)
{
	void *mem;		// 页面的内核虚拟地址
	int err = 0;

	if (op_is_write(op))
	{
		err = copy_to_brd_setup(brd, sector, len);
		if (err)
		{
			goto out;
		}
	}

	mem = kmap_atomic(page);	// 原子映射bio的页面
	if (!op_is_write(op))
	{
		copy_from_brd(mem + off, brd, sector, len);
	}
	else
	{
		flush_dcache_page(page);	// 先刷新数据缓存, 确保bio页面的最新数据对brd可见
		copy_to_brd(brd, mem + off, sector, len);
	}
	kunmap_atomic(mem);

out:
	return err;
}




#ifndef MODULE
/* Legacy boot options - nonmodular 传统启动选项 - 非模块化时使用 */
static int __init ramdisk_size(char *str) // 解析内核启动参数 "ramdisk_size=" 的函数
{
	rd_size = simple_strtol(str, NULL, 0); // 将字符串转换为长整型，设置全局 rd_size
	return 1;
}
__setup("ramdisk_size=", ramdisk_size); // 注册内核启动参数处理函数
#endif

/**
 * @brief 查找或分配一个指定编号的brd设备
 * @param 
 * @note
 */
static struct brd_device *brd_find_or_alloc_device(int i)
{
	struct brd_device *brd;

	/*上锁*/
	mutex_lock(&brd_devices_mutex);
	/*查找*/
	list_for_each_entry(brd, &brd_devices, brd_list) {
		if (brd->brd_number == i)
		{
			mutex_unlock(&brd_devices_mutex);
			return ERR_PTR(-EEXIST);
		}
	}

	brd = kzalloc(sizeof(*brd), GFP_KERNEL);
	if (!brd)
	{
		mutex_unlock(&brd_devices_mutex);
		return ERR_PTR(-ENOMEM);
	}
	brd->brd_number = i;		// 设置设备编号
	list_add_tail(&brd->brd_list, &brd_devices);
	mutex_unlock(&brd_devices_mutex);

	return brd;
}

/**
 * @brief 释放一个brd设备
 * @param 
 * @note
 */
static void brd_free_device(struct brd_device *brd)
{
	mutex_lock(&brd_devices_mutex);
	list_del(&brd->brd_list);		// 从链表中删除
	mutex_unlock(&brd_devices_mutex);
	kfree(brd);
}

/**
 * @brief 分配并初始化一个完整的brd设备
 * @param 
 * @note
 */
static int brd_alloc(int i)
{
	struct brd_device *brd;
	struct gendisk *disk;
	char buf[DISK_NAME_LEN];	// 磁盘名称缓冲区
	int err = -ENOMEM;

	brd = brd_find_or_alloc_device(i);
	if (IS_ERR(brd))
	{
		return PTR_ERR(brd);
	}

	/*初始化互斥锁*/
	spin_lock_init(&brd->brd_lock);		// 管理页面的时候需要
	/*基数树, 管理物理页面*/
	INIT_RADIX_TREE(&brd->brd_pages, GFP_KERNEL);

	snprintf(buf, DISK_NAME_LEN, "ram%d", i);
	if (!IS_ERR_OR_NULL(brd_debugfs_dir))
	{
		debugfs_create_u64(buf, 0444, brd_debugfs_dir,
				&brd->brd_nr_pages);
	}

	/*分配一个磁盘结构体*/
	disk = brd->brd_disk = blk_alloc_disk(NUMA_NO_NODE);
	if (!disk)
	{
		goto out_free_dev;
	}

	disk->major = RAMDISK_MAJOR;
	disk->first_minor = i * max_part;	// 起始从设备号
	disk->minors = max_part;			// 设备从设备号数量
	disk->fops = &brd_fops;				// 设置块设备操作函数
	disk->private_data = brd;			// 设置私有数据
	disk->flags = GENHD_FL_EXT_DEVT;	// 支持扩展设备号
	strlcpy(disk->disk_name, buf, DISK_NAME_LEN);

	/*设置容量: 以扇区为单位, 扇区为512字节*/
	set_capacity(disk, rd_size * 2);	// 设置磁盘容量扇区数


	/*请求队列设置*/
	/*配置物理页大小为一个PAGE的大小*/
	blk_queue_physical_block_size(disk->queue, PAGE_SIZE); // 设置物理块大小为 PAGE_SIZE

	blk_queue_flag_set(QUEUE_FLAG_NONROT, disk->queue); // 设置非旋转标志 (SSD/RAM)
	blk_queue_flag_clear(QUEUE_FLAG_ADD_RANDOM, disk->queue); // 清除“为随机数池增加熵”的标志
	blk_queue_flag_set(QUEUE_FLAG_NOWAIT, disk->queue); // 设置“无等待”标志，表示请求处理不会阻塞

	/*向内核注册磁盘设备*/
	err = add_disk(disk);
	if (err)
	{
		goto out_cleanup_disk;
	}
	return 0;	// 成功

out_cleanup_disk:
	blk_cleanup_disk(disk);		// 清理gendisk结构体
out_free_dev:
	brd_free_device(brd);		// 释放设备结构体
	return err;
}


/*****************************************************
 *                  设备操作
 ******************************************************/
/**
 * @brief 提交bio请求的处理函数: 这是通用块层的关键接口
 * @param 
 * @note 通用块层
 */
static blk_qc_t brd_submit_bio(struct bio *bio)
{
	/*从块设备中获取结构体数据*/
	struct brd_device *brd = bio->bi_bdev->bd_disk->private_data;
	sector_t sector = bio->bi_iter.bi_sector;	// 获取bio的起始扇区号
	struct bio_vec bvec;						// 用于迭代bio_vec
	struct bvec_iter iter;						// 迭代器

	bio_for_each_segment(bvec, bio, iter) {
		unsigned int len = bvec.bv_len;			// 当前段的长度
		int err;

		WARN_ON_ONCE((bvec.bv_offset & (SECTOR_SIZE - 1)) || // 偏移量必须扇区对齐
			(len & (SECTOR_SIZE -1)));						 // 长度必须扇区对齐
		
		/*处理这个段*/
		err = brd_do_bvec(brd, bvec.bv_page, len, bvec.bv_offset, bio_op(bio), sector);
		if (err)
		{
			goto io_error;
		}

		sector += len >> SECTOR_SHIFT;	// 更新扇区, 指向下一个段
	}

	bio_endio(bio);
	return BLK_QC_T_NONE;			// 返回空队列命令标签
io_error:
	bio_io_error(bio);
	return BLK_QC_T_NONE;
}

/**
 * @brief 读写一个整页
 * @param 
 * @note
 */
static int brd_rw_page(struct block_device *bdev, sector_t sector,
	struct page *page, unsigned int op)
{
	struct brd_device *brd = bdev->bd_disk->private_data;
	int err;

	if (PageTransHuge(page))
	{
		return -ENOTSUPP;
	}
	/*处理整页读写*/
	err = brd_do_bvec(brd, page, PAGE_SIZE, 0, op, sector);
	page_endio(page, op_is_write(op), err);
	return err;
}


/*****************************************************
 *                  驱动操作
 ******************************************************/
static void brd_probe(dev_t dev)
{
	brd_alloc(MINOR(dev) / max_part);	// 根据从设备号计算设备索引并分配设备
}

static void brd_cleanup(void)
{
	struct brd_device *brd, *next;

	debugfs_remove_recursive(brd_debugfs_dir);
	
	list_for_each_entry_safe(brd, next, &brd_devices, brd_list) {
		/*删除gendisk*/
		del_gendisk(brd->brd_disk);
		/*清理disk资源*/
		blk_cleanup_disk(brd->brd_disk);

		brd_free_pages(brd);
		brd_free_device(brd);
	}
}

/**
 * @brief 检查并重置max_part, 确保其合法
 * @param 
 * @note
 */
static inline void brd_check_and_reset_par(void)
{
	if (unlikely(!max_part))
	{
		max_part = 1;
	}

	if ((1U << MINORBITS) % max_part != 0) // 如果不能整除
	{
		max_part = 1UL << fls(max_part); // 将 max_part 调整为大于等于它的最小 2 的幂
	}

	if (max_part > DISK_MAX_PARTS) 
	{ // 如果 max_part 超过内核允许的最大分区数
		pr_info("brd: max_part can't be larger than %d, reset max_part = %d.\n",
			DISK_MAX_PARTS, DISK_MAX_PARTS);
		max_part = DISK_MAX_PARTS; // 将其重置为最大分区数
	}
}
/*****************************************************
 *                  模块初始化和退出
 ******************************************************/
static int __init ram_disk_init(void)
{
	int err, i;

	brd_check_and_reset_par();	// 检查并调整参数
	
	brd_debugfs_dir = debugfs_create_dir("ramdisk_pages", NULL);

	/*注册块设备*/
	if (__register_blkdev(RAMDISK_MAJOR, "ramdisk", brd_probe)) 
	{
		err = -EIO;			// 注册失败
		goto out_free;
	}

	for (i = 0; i < rd_nr; ++i)
	{
		brd_alloc(i);
	}

	pr_info("brd: module loaded\r\n"); // 打印模块加载信息
	return 0;
out_free:
	brd_cleanup();	// 执行清理工作
	pr_info("brd: module NOT loaded !!!\r\n");
	return err;
}

static void __exit ram_disk_exit(void)
{
	/*注册块设备*/
	unregister_blkdev(RAMDISK_MAJOR, "ramdisk"); // 注销块设备主设备号
	brd_cleanup(); // 清理所有设备

	pr_info("brd: module unloaded\r\n"); // 打印模块卸载信息
}


module_init(ram_disk_init);
module_exit(ram_disk_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

