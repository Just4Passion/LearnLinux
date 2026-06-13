
# AI提示词
```

```


# 字符设备驱动
    - 摘要
        - 一切皆文件, 设备也是文件
        - VFS: 虚拟文件系统. 
        - 设备分类
            - 字符设备: struct cdev
            - 块设备: struct block_device
            - 网络设备: struct net_devce
        - 字符设备适配VFS
            - 创建一个struct cdev
            - 创建一个文件
            - 建立二者之间的绑定关系
        - 主设备号和次设备号
            - 主设备号: 区分设备类别. 一般情况, 主设备号指向设备的驱动程序, 同类设备都共用同一套驱动程序
            - 次设备号: 标识具体的设备
        - 设备
            - 一切皆设备
            - 层次化组织: 通过parent形成设备树
            - 统一接口: 为不同总线类型的设备提供统一的操作接口

## 项目简介: 这个项目是做什么的, 组成是什么, 如何编译构建, 如何使用
1. **目的**
    - 学习字符设备驱动如何开发和使用
2. **组成**
    - char_device.c: 字符设备驱动
    - test_demo.c: 驱动测试demo
    - Makefile: 构建脚本
3. **如何构建**
    - make: 编译生成模块, 生成测试程序
    - make clean: 清理生成文件
4. **使用**
    - 安装驱动
        - 
    - 执行测试程序
        - 

# 字符设备开发

## 概念简介
1. **struct device**: linux统一设备抽象模型 - 数据(硬件资源) + 驱动 + 总线 + 缓冲区
```
#include <linux/device.h>
struct device {
	struct kobject kobj;
	struct device		*parent;

......
	struct bus_type	*bus;		            /* type of bus device is on */
	struct device_driver *driver;	        /* which driver has allocated this device */
	void		*platform_data;	            /* Platform specific data, device core doesn't touch it */
	void		*driver_data;	            /* Driver data, set and get with dev_set_drvdata/dev_get_drvdata */
......
	struct mutex		mutex;	/* mutex to synchronize calls to
					 * its driver.
					 */
......
	struct dev_links_info	links;
	struct dev_pm_info	power;
	struct dev_pm_domain	*pm_domain;

......

	struct device_node	*of_node;       /* associated device tree node */
	struct fwnode_handle	*fwnode;    /* firmware device node */
......
	dev_t			devt;	            /* dev_t, creates the sysfs "dev" */
	u32			id;	                    /* device instance */

	spinlock_t		devres_lock;
	struct list_head	devres_head;
......
	struct class		*class;
......
	void	(*release)(struct device *dev);
	struct iommu_group	*iommu_group;
	struct dev_iommu	*iommu;

};
```

2. **struct cdev**: VFS层的设备抽象 - 文件操作函数struct file_operations *ops
```
#include <linux/cdev.h>
struct cdev {
	struct kobject kobj;
	struct module *owner;
	const struct file_operations *ops;
	struct list_head list;
	dev_t dev;
	unsigned int count;
} __randomize_layout;
```
3. **设备号**: dev_t, 32位的数, 高12位表示主设备号，低20位表示次设备号
    - 主设备号
        - cat /proc/devices, 可以查看主设备号
    - 次设备号
        - 主设备号下的子设备

4. **文件操作函数**
```
#include <linux/fs.h>

struct file_operations {
	struct module *owner;
	loff_t (*llseek) (struct file *, loff_t, int);
	ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
	ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
	ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
	ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
	int (*iopoll)(struct kiocb *kiocb, bool spin);
	int (*iterate) (struct file *, struct dir_context *);
	int (*iterate_shared) (struct file *, struct dir_context *);
	__poll_t (*poll) (struct file *, struct poll_table_struct *);
	long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
	long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
	int (*mmap) (struct file *, struct vm_area_struct *);
	unsigned long mmap_supported_flags;
	int (*open) (struct inode *, struct file *);
	int (*flush) (struct file *, fl_owner_t id);
	int (*release) (struct inode *, struct file *);
	int (*fsync) (struct file *, loff_t, loff_t, int datasync);
	int (*fasync) (int, struct file *, int);
	int (*lock) (struct file *, int, struct file_lock *);
	ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
	unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
	int (*check_flags)(int);
	int (*flock) (struct file *, int, struct file_lock *);
	ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
	ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
	int (*setlease)(struct file *, long, struct file_lock **, void **);
	long (*fallocate)(struct file *file, int mode, loff_t offset,
			  loff_t len);
	void (*show_fdinfo)(struct seq_file *m, struct file *f);
#ifndef CONFIG_MMU
	unsigned (*mmap_capabilities)(struct file *);
#endif
	ssize_t (*copy_file_range)(struct file *, loff_t, struct file *,
			loff_t, size_t, unsigned int);
	loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in,
				   struct file *file_out, loff_t pos_out,
				   loff_t len, unsigned int remap_flags);
	int (*fadvise)(struct file *, loff_t, loff_t, int);

} __randomize_layout;
```

## 字符设备驱动程序框架
```
用户空间
---------------------------------
|          应用程序              |
|  (使用标准文件I/O操作)         |
|   open() / read() / write()    |
|   ioctl() / close() 等         |
---------------------------------
         | (系统调用接口)
         V
内核空间
---------------------------------
|     虚拟文件系统 (VFS)         |
| 提供统一文件操作接口            |
---------------------------------
         | (调用驱动注册的操作函数)
         V
---------------------------------
|     字符设备驱动框架           |
---------------------------------
         |
         +-- 初始化阶段
         |   |
         |   +-- module_init()  --> 驱动入口点
         |       |
         |       +-- alloc_chrdev_region()  /  register_chrdev_region()
         |       |    |--> 动态/静态分配设备号
         |       |
         |       +-- cdev_init()  --> 初始化cdev结构体
         |       |    |
         |       |    +-- 绑定 file_operations
         |       |
         |       +-- cdev_add()   --> 添加设备到系统
         |       |
         |       +-- class_create()  --> 创建设备类
         |       |
         |       +-- device_create() --> 创建设备节点
         |
         +-- 操作函数集 (file_operations)
         |   |
         |   +-- .open    = chrdev_open   --> 设备打开
         |   |
         |   +-- .release = chrdev_release --> 设备关闭
         |   |
         |   +-- .read    = chrdev_read    --> 读取设备数据
         |   |
         |   +-- .write   = chrdev_write   --> 写入设备数据
         |   |
         |   +-- .unlocked_ioctl = chrdev_ioctl --> 设备控制
         |   |
         |   +-- .llseek  = chrdev_llseek  --> 定位设备位置
         |
         +-- 硬件操作层
         |   |
         |   +-- ioremap() / ioremap_nocache()  --> 映射物理内存
         |   |
         |   +-- request_irq()  --> 注册中断处理函数
         |   |
         |   +-- 硬件寄存器读写操作
         |   |
         |   +-- 数据传输函数 (如DMA操作)
         |
         +-- 退出阶段
             |
             +-- module_exit()  --> 驱动退出点
                 |
                 +-- device_destroy()    --> 销毁设备节点
                 |
                 +-- class_destroy()     --> 销毁设备类
                 |
                 +-- cdev_del()          --> 删除cdev
                 |
                 +-- unregister_chrdev_region() --> 释放设备号
```

## init, open, write, read





## 开发流程
1. **init**
    - 申请设备号
    - 创建cdev并初始化: 绑定设备号, 绑定struct file_operations
    - 注册cdev到cdev_map
    - 创建class: 通过module句柄创建一个设备类
    - 创建device: class, 设备号, device完成绑定
2. **exit**
    - 删除cdev
    - 释放设备号
    - 销毁device
    - 销毁class

## 多个次设备开发
1. **多个cdev分别管理**
    - 每个次设备创建一个cdev, 并注册；每个设备号创建一个设备节点device, 设备节点通过序列号区分
2. **单个cdev管理多个次设备**
    - 一次分配多个devno, 通过cdev_add一次性添加多个次设备
    - 每个次设备绑定一个device，设备节点通过序列号区分




# 问题
    - 前置配置
        - ulimit -c, 查看coredump配置
        - ulimit -c unlimited, 启用coredump
        - /etc/sys
    - 调试方法
        - kgdb调试
    - 地址定位
        - 通过dmesg查看崩溃信息
        - 根据崩溃点的地址, 使用addr2line -e char_device.ko -f -C 0x3f(崩溃偏移地址)
        - 根据cat /sys/module/char_device/sections/.text, 找到模块加载地址
        - 或者使用addr2line -e char_device.ko -f -C (加载地址 + 偏移地址)

## 崩溃排查


