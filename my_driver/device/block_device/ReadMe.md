

# block_device简介

## 项目
```
缓冲，IO调度，请求队列
MMC子系统以及它与块设备


代码逻辑
1. 注册和注销块设备
    - register_blkdev
    - unregister_blkdev
2. 分配并初始化一个gendisk
    - struct gendisk *alloc_disk(int minors)
    - void del_gendisk(struct gendisk *gp)
    - void add_disk(struct gendisk *disk)
    - void set_capacity(struct gendisk *disk, sector_t size)
    - 引用计数: 内核调用
        - struct kobject * get_disk_and_module (struct gendisk *disk)
        - void put_disk(struct gendisk *disk)
3. 块设备I/O请求过程
    - request_queue
        - blk_mq_alloc_tag_set -> blk_mq_tag_set
        - blk_mq_init_queue
        - blk_cleanup_queue
        - void blk_queue_make_request(struct request_queue *q, make_request_fn *mfn)
    - request
        - void blk_mq_start_request(struct request *rq)
        - void blk_mq_end_request(struct request *rq, blk_status_t error)
```



# 模块简介
    - 存储设备: SD卡, EMMC, NAND Flash, Nor Flash, SPI Flash, 机械硬盘, 固态硬盘

## 块设备
1. **I/O操作特点**
  - 只能以块为单位接收输入和返回输出
  - 对于I/O请求有对应的缓冲区: 可以选择以什么顺序进行响应
  - 可以随机访问
2. **分层架构**
  - VFS
  - 文件系统; 原始块设备
  - 块I/O调度层
  - 块设备驱动
3. **struct block_device_operations**
```
struct block_device_operations {
	blk_qc_t (*submit_bio) (struct bio *bio);
  /******************************************
                  打开和释放
  ******************************************/
	int (*open) (struct block_device *, fmode_t);
	void (*release) (struct gendisk *, fmode_t);
	int (*rw_page)(struct block_device *, sector_t, struct page *, unsigned int);
  /******************************************
                  I/O控制
  ******************************************/
	int (*ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
	int (*compat_ioctl) (struct block_device *, fmode_t, unsigned, unsigned long);
	unsigned int (*check_events) (struct gendisk *disk,
				      unsigned int clearing);
	void (*unlock_native_capacity) (struct gendisk *);
	int (*revalidate_disk) (struct gendisk *);
  /******************************************
                获取驱动信息
  ******************************************/
	int (*getgeo)(struct block_device *, struct hd_geometry *);
	/* this callback is with swap_lock and sometimes page table lock held */
	void (*swap_slot_free_notify) (struct block_device *, unsigned long);
	int (*report_zones)(struct gendisk *, sector_t sector,
			unsigned int nr_zones, report_zones_cb cb, void *data);
	char *(*devnode)(struct gendisk *disk, umode_t *mode);
	struct module *owner;
	const struct pr_ops *pr_ops;

	ANDROID_KABI_RESERVE(1);
	ANDROID_KABI_RESERVE(2);
	ANDROID_OEM_DATA(1);
};
```
4. **struct gendisk**: 表示一个独立的磁盘设备或分区
```
struct gendisk {
	/******************************************
                  主次设备号
    ******************************************/
	int major;			/* major number of driver */
	int first_minor;
	int minors;                     /* maximum number of minors, =1 for
                                         * disks that can't be partitioned. */

	char disk_name[DISK_NAME_LEN];	/* name of major driver */

	unsigned short events;		/* supported events */
	unsigned short event_flags;	/* flags related to event processing */

	/* Array of pointers to partitions indexed by partno.
	 * Protected with matching bdev lock but stat and other
	 * non-critical accesses use RCU.  Always access through
	 * helpers.
	 */
    /******************************************
                  磁盘对应的分区表
    ******************************************/
	struct disk_part_tbl __rcu *part_tbl;
	struct hd_struct part0;

    /******************************************
                  块设备操作
    ******************************************/
	const struct block_device_operations *fops;
    /******************************************
                  I/O请求队列
    ******************************************/
	struct request_queue *queue;
	void *private_data;

	int flags;
	unsigned long state;
#define GD_NEED_PART_SCAN		0
	struct rw_semaphore lookup_sem;
	struct kobject *slave_dir;

	struct timer_rand_state *random;
	atomic_t sync_io;		/* RAID */
	struct disk_events *ev;
#ifdef  CONFIG_BLK_DEV_INTEGRITY
	struct kobject integrity_kobj;
#endif	/* CONFIG_BLK_DEV_INTEGRITY */
#if IS_ENABLED(CONFIG_CDROM)
	struct cdrom_device_info *cdi;
#endif
	int node_id;
	struct badblocks *bb;
	struct lockdep_map lockdep_map;

	ANDROID_KABI_RESERVE(1);
	ANDROID_KABI_RESERVE(2);
	ANDROID_KABI_RESERVE(3);
	ANDROID_KABI_RESERVE(4);

};
```

5. **struct block_device**: 块设备
```
struct block_device {
	dev_t			bd_dev;
	int			bd_openers;
  /******************************************
                VFS文件节点
  ******************************************/
	struct inode *		bd_inode;	/* will die */
  /******************************************
            超级块: 记录了所有节点的信息
  ******************************************/
	struct super_block *	bd_super;
	struct mutex		bd_mutex;	/* open/close mutex */
	void *			bd_claiming;
	void *			bd_holder;
	int			bd_holders;
	bool			bd_write_holder;
#ifdef CONFIG_SYSFS
	struct list_head	bd_holder_disks;
#endif
  /******************************************
                  块设备
  ******************************************/
	struct block_device *	bd_contains;
	u8			bd_partno;
	struct hd_struct *	bd_part;
	/* number of times partitions within this device have been opened. */
	unsigned		bd_part_count;

	spinlock_t		bd_size_lock; /* for bd_inode->i_size updates */
  /******************************************
                  分区设备
  ******************************************/
	struct gendisk *	bd_disk;
	struct backing_dev_info *bd_bdi;

	/* The counter of freeze processes */
	int			bd_fsfreeze_count;
	/* Mutex for freeze */
	struct mutex		bd_fsfreeze_mutex;
	struct super_block	*bd_fsfreeze_sb;

	ANDROID_KABI_RESERVE(1);
	ANDROID_KABI_RESERVE(2);
	ANDROID_KABI_RESERVE(3);
	ANDROID_KABI_RESERVE(4);
} __randomize_layout;
```
6. **bio**：request包含多个bio, request是bio经过I/O调度的结果
```
struct bio {
  /******************************************
                  分区设备
  ******************************************/
	struct bio		*bi_next;	/* request queue link */
  /******************************************
                  分区设备
  ******************************************/
	struct gendisk		*bi_disk;
	unsigned int		bi_opf;
	unsigned short		bi_flags;	/* status, etc and bvec pool number */
	unsigned short		bi_ioprio;
	unsigned short		bi_write_hint;
	blk_status_t		bi_status;
	u8			bi_partno;
	atomic_t		__bi_remaining;
  /******************************************
              I/O请求的开始扇区
  ******************************************/
	struct bvec_iter	bi_iter;

	bio_end_io_t		*bi_end_io;

	void			*bi_private;
  ......

	unsigned short		bi_max_vecs;	/* max bvl_vecs we can hold */

	atomic_t		__bi_cnt;	/* pin count */

  /******************************************
                指向数据放入的页
    bio对应的数据每次存放的内存不一定是连续的
    这个结构体用来描述与这个bio请求对应的所有内存
    由于数据可能存放在不同的页，所以需要一个链表
  ******************************************/
	struct bio_vec		*bi_io_vec;	/* the actual vec list */

	struct bio_set		*bi_pool;

	ANDROID_KABI_RESERVE(1);
	ANDROID_KABI_RESERVE(2);

	/*
	 * We can inline a number of vecs at the end of the bio, to avoid
	 * double allocations for a small number of bio_vecs. This member
	 * MUST obviously be kept at the very end of the bio.
	 */
	struct bio_vec		bi_inline_vecs[];
};
```

7. **I/O调度器**: 4个I/O调度器
    - Noop I/O调度器
        - FIFO队列, 简单合并, 适合Flash存储器
    - Anticipatory I/O调度器
        - 推迟I/O请求, 进行排序
    - Deadline I/O调度器
        - 优化Anticipatory, 降低延迟, 重排请求的顺序来提高性能
    - CFQ I/O调度器 
        - 为系统内的所有任务分配均匀的I/O带宽


