
# Linux驱动架构简介
    - 核心思想: 分层, 分隔(把M*N变成M+N - 也就是通过中间件把两个东西强关联的东西分隔开来)
        - 总线, 驱动, 设备
        - 驱动与设备分离
        - 驱动分层: 核心层, 设备底层
            - **核心层**: 统一的, 与具体设备驱动无关的操作
            - **设备底层**: 与设备强绑定的行为
        - 主机与外设驱动分离
    - 驱动实例
        - platform驱动
        - RTC驱动
        - Framebuffer驱动
        - input子系统   
            - 所有的输入设备, 都需要接入file_operation, 区分不同的I/O模型(阻塞/非阻塞, 同步/异步)
        - tty驱动
        - 混杂设备驱动: misc
        - SPI主机与外设驱动

1. **设备驱动分层思想**
    - Linux会为同类的设备设计一个框架, 框架的核心层则实现该设备一些通用的功能
    - 具体的设备, 不想使用核心层的功能, 可以**重写**
    - **一些特定的底层操作, 可以通过具体设备的ops实现**
2. **驱动核心层**
    - 向上提供接口. file_operations的读，写，ioctl都被中间层搞定, 各种I/O模型也被处理掉了
    - 中间层实现通用逻辑. 
    - 对下定义框架. 实现与具体硬件相关的预留的接口即可
3. **主机驱动与外设驱动分离**
    - SPI, I2C, USB等总线系统
        - 主机端只负责产生总线上的传输波形, 外设端通过**标准的API**来让主机端以适当的波形访问自身
        - 4个软件模块
            - 主机端的驱动: 根据具体的总线控制器的硬件手册, 操作具体的控制器, 产生总线的各种波形
            - 连接主机和外设纽带: 外设调用**标准的API**, API把波形的传输请求间接转发给具体的主机端驱动. 波形数据以某种结构化数据描述
            - 外设端驱动: 通过xxx_driver.probe()进行注册. 访问外设时, 调用标准的API
            - 板级逻辑: 多个控制器和多个外设是如何互联的. 通过DTS设备树来描述

# 字符设备驱动中间层

## platform总线、设备、驱动
    - 物理世界中有很多总线, 设备模型的架构
        - SPI总线
        - I2C总线
        - USB总线
        - PCI总线
        - I2C总线
        - SDIO总线
    - platform总线
        - 在SoC中, 一些控制器集成在SoC上, 但是它们并不具备具体的总线, 为了适应Linux的设备驱动模型, 抽象出一种总线叫做platform总线
        - platform驱动: platform_driver
        - platform设备: platform_device
    - SPI总线
        - spi驱动: spi_driver
        - spi设备: spi_device

1. **总线**: 将设备与驱动绑定(每注册一个驱动, 寻找寻找与之匹配的设备; 每注册一个设备, 寻找与之匹配的驱动)
2. **platform总线**: 虚拟总线
    - 管理platform_device和platform_driver
3. **platform_device**
    - SoC中集成的独立外设控制器、挂接在SoC内存空间的外设
    - SoC内部集成的I2C、RTC、LCD、看门狗等控制器, 都归纳为paltform_device, 其本身都是字符设备
```
struct platform_device {
	const char	*name;
	int		id;
	bool		id_auto;
	struct device	dev;
	u64		platform_dma_mask;
	struct device_dma_parameters dma_parms;
	u32		num_resources;
	struct resource	*resource;

	const struct platform_device_id	*id_entry;
	char *driver_override;

	/* MFD cell pointer */
	struct mfd_cell *mfd_cell;

	/* arch specific additions */
	struct pdev_archdata	archdata;
};
```
4. **platform_driver**
    - 与之对等的有: i2c_driver, spi_driver, usb_driver, pci_driver
```
struct platform_driver {
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	void (*shutdown)(struct platform_device *);
	int (*suspend)(struct platform_device *, pm_message_t state);
	int (*resume)(struct platform_device *);
	struct device_driver driver;                //基础设备模型, 通过dev_pm_ops结构体成员做电源管理
	const struct platform_device_id *id_table;
	bool prevent_deferred_probe;
};
```

## Input子系统设备驱动(分层): 核心层 + 输入设备驱动程序. 
    - 输入设备
        - 键盘, 按键, 触摸屏, 鼠标等的工作原理
            - 底层动作触发一个中断(或驱动通过定时器查询), CPU通过SPI、I2C或外部存储器总线读取数据, 并把它们放在一个缓冲区
            - 字符设备驱动管理这个缓冲区, 驱动的read()接口让用户可以读取这些数据
        - 特性: 中断, 读取外设输入. 
        - 共性: 输入事件的缓冲区管理, 字符设备驱动的file_operations接口是通用的
1. **设备注册**
    - 分配/释放一个输入设备
        - struct input_dev *input_allocate_device(void);
        - void input_free_device(struct input_dev *dev);
    - 注册/注销一个输入设备
        - int __must_check input_register_device(struct input_dev *dev);
        - void input_unregister_device(struct input_dev *dev);
2. **报告输入事件**
    - void input_event(): 报告事件
    - void input_report_key(): 报告键值
    - void input_report_rel(): 报告相对坐标
    - void input_report_abs(): 报告绝对坐标
    - void input_sync(): 报告同步事件
```事件的统一数据结构
struct input_event {
    struct timeval time;
    __u16 type;
    __u16 code;
    __s32 value;
};
```
3. **中断处理函数**: 在中断处理函数中报告输入事件
4. **核心层的file_operations**
```
static const struct file_operations evdev_fops = {
    .owner = THIS_MODULE,
    .read = evdev_read,
    .write = evdev_write,
    .pol = evdev_poll,
    .open = evdev_open,
    .release = evdev_release,
    .unlocked_ioctl = evdev_ioctl,
    .fasync = evdev_fasync,
    .flush = evdev_flush,
    .llseek = no_llseek,
};
```

## RTC设备驱动: 字符设备驱动. 计时, 周期性中断, 闹钟中断 —— rtc_class_ops, 硬件特化
1. **RTC底层硬件操作rtc_class_ops**
```具体的RTC硬件实例
static const struct rtc_class_ops s3c_rtcops = {
    .read_time = s3c_rtc_gettime,
    .set_time = s3c_rtc_settime,
    .read_alarm = s3c_rtc_getalarm,
    .set_alarm = s3c_rtc_setalarm,
    .proc = s3c_rtc_proc,
    .alarm_irq_enable = s3c_rtc_setaie
};
```
2. **RTC核心层file_operations**: 调用rtc_class_ops
    - ioctl
        - RTC_SET_TIME
        - RTC_ALM_READ
        - RTC_ALM_SET
        - RTC_IRQP_SET
        - RTC_IRQP_READ

## Framebuffer设备驱动: Linux系统为显示设备提供的一个接口 —— fb_ops, 硬件特化
    - 显示缓冲区
1. **显示设备核心struct fb_info**: 
```
struct fb_info {
	atomic_t count;
	int node;
	int flags;
	struct mutex lock;		/* Lock for open/release/ioctl funcs */
	struct mutex mm_lock;		/* Lock for fb_mmap and smem_* fields */
	struct fb_var_screeninfo var;	/* Current var */
	struct fb_fix_screeninfo fix;	/* Current fix */
	struct fb_monspecs monspecs;	/* Current Monitor specs */
	struct work_struct queue;	/* Framebuffer event queue */
	struct fb_pixmap pixmap;	/* Image hardware mapper */
	struct fb_pixmap sprite;	/* Cursor hardware mapper */
	struct fb_cmap cmap;		/* Current cmap */
	struct list_head modelist;      /* mode list */
	struct fb_videomode *mode;	/* current mode */

#ifdef CONFIG_FB_BACKLIGHT  //背光
	struct backlight_device *bl_dev;

	struct mutex bl_curve_mutex;	
	u8 bl_curve[FB_BACKLIGHT_LEVELS];
#endif
#ifdef CONFIG_FB_DEFERRED_IO
	struct delayed_work deferred_work;
	struct fb_deferred_io *fbdefio;
#endif

	struct fb_ops *fbops;
	struct device *device;		/* This is the parent */
	struct device *dev;		/* This is this fb device */
	int class_flag;                    /* private sysfs flags */
    ...
	union {
		char __iomem *screen_base;	/* Virtual address */
		char *screen_buffer;
	};
	unsigned long screen_size;	/* Amount of ioremapped VRAM or 0 */ 
	void *pseudo_palette;		/* Fake palette of 16 colors */ 
    ...
};
```
2. **显示硬件相关的操作struct fb_ops**
```
struct fb_ops {
	/* open/release and usage marking */
	struct module *owner;
	int (*fb_open)(struct fb_info *info, int user);
	int (*fb_release)(struct fb_info *info, int user);

	/* For framebuffers with strange non linear layouts or that do not
	 * work with normal memory mapped access
	 */
	ssize_t (*fb_read)(struct fb_info *info, char __user *buf,
			   size_t count, loff_t *ppos);
	ssize_t (*fb_write)(struct fb_info *info, const char __user *buf,
			    size_t count, loff_t *ppos);

	/* checks var and eventually tweaks it to something supported,
	 * DO NOT MODIFY PAR */
	int (*fb_check_var)(struct fb_var_screeninfo *var, struct fb_info *info);

	/* set the video mode according to info->var */
	int (*fb_set_par)(struct fb_info *info);

	/* set color register */
	int (*fb_setcolreg)(unsigned regno, unsigned red, unsigned green,
			    unsigned blue, unsigned transp, struct fb_info *info);

	/* set color registers in batch */
	int (*fb_setcmap)(struct fb_cmap *cmap, struct fb_info *info);

	/* blank display */
	int (*fb_blank)(int blank, struct fb_info *info);

	/* pan display */
	int (*fb_pan_display)(struct fb_var_screeninfo *var, struct fb_info *info);

	/* Draws a rectangle */
	void (*fb_fillrect) (struct fb_info *info, const struct fb_fillrect *rect);
	/* Copy data from area to another */
	void (*fb_copyarea) (struct fb_info *info, const struct fb_copyarea *region);
	/* Draws a image to the display */
	void (*fb_imageblit) (struct fb_info *info, const struct fb_image *image);

	/* Draws cursor */
	int (*fb_cursor) (struct fb_info *info, struct fb_cursor *cursor);

	/* Rotates the display */
	void (*fb_rotate)(struct fb_info *info, int angle);

	/* wait for blit idle, optional */
	int (*fb_sync)(struct fb_info *info);

	/* perform fb specific ioctl (optional) */
	int (*fb_ioctl)(struct fb_info *info, unsigned int cmd,
			unsigned long arg);

	/* Handle 32bit compat ioctl (optional) */
	int (*fb_compat_ioctl)(struct fb_info *info, unsigned cmd,
			unsigned long arg);

	/* perform fb specific mmap */
	int (*fb_mmap)(struct fb_info *info, struct vm_area_struct *vma);

	/* get capability given var */
	void (*fb_get_caps)(struct fb_info *info, struct fb_blit_caps *caps,
			    struct fb_var_screeninfo *var);

	/* teardown any resources to do with this framebuffer */
	void (*fb_destroy)(struct fb_info *info);

	/* called at KDB enter and leave time to prepare the console */
	int (*fb_debug_enter)(struct fb_info *info);
	int (*fb_debug_leave)(struct fb_info *info);

	/* Export the frame buffer as a dmabuf object */
	struct dma_buf *(*fb_dmabuf_export)(struct fb_info *info);
};
```
3. **核心层的file_operations**
    - fb_read()
    - fb_write()
    - fb_mmap()
    - fb_ioctl()



## 终端设备驱动: tty. tty_operations
    - 核心组成
        - tty_io.c: 字符设备驱动, 实现file_operations
        - n_tty.c: N_TTY线路规程 —— 以特殊的方式格式化从一个用户或硬件收到的数据
        - tty_driver: tty_operations. 填充tty_operations成员
    - 数据通路
        - 用户数据 -> tty核心 -> tty线路规程 -> tty驱动 -> 硬件
        - 硬件 -> tty驱动 -> tty线路规程 -> tty核心 -> 用户空间
    - UART设备的通用tty驱动层
        - drivers/tty/serial/serial_core.c
        - uart_driver: uart_ops

## MISC设备驱动: misc. 字符设备, 主设备号固定为10






# 总线型设备
    - 主机端驱动: 总线控制器驱动, 只负责产生波形. 与芯片的硬件强相关
    - 外设驱动: 通过调用**标准API**访问主机端驱动
    - 纽带: 主机驱动与外设驱动通信的数据结构 + 标准API接口
    - 总线与外设的互联逻辑: 设备树 

## SPI主机和设备驱动
     - spi controller: 提供SPI传输的接口
        - 当需要移植新的SoC时, 需要实现这个SoC的spi_master或者说是spi_controller中定义的接口
            - transfer
            - set_cs
            - transfer_one
            - handle_err
            - ...
     - spi driver: 通过标准API调用调用controller的传输接口
     - spi message: 待传输的数据的软件抽象模型
     - spi device: 挂载在具体SPI总线上的设备
     - spi board info: 子设备的板载信息
     - spi dts: 设备树中子设备的描述
     - spi总线
```
struct bus_type spi_bus_type = {
	.name		= "spi",
	.dev_groups	= spi_dev_groups,
	.match		= spi_match_device,
	.uevent		= spi_uevent,
};
EXPORT_SYMBOL_GPL(spi_bus_type);
```
1. **struct spi_controller**: SPI主机控制器. define spi_master spi_controller
    - 主机控制器的序号
    - 片选数量
    - SPI模式: 极性和相位
    - 时钟设置
    - 数据传输
```
struct spi_controller {
    struct device	dev;

	struct list_head list;

	s16			bus_num;

    /********************************
                互斥访问
    *********************************/
    /* I/O mutex */
	struct mutex		io_mutex;
	/* lock and mutex for SPI bus locking */
	spinlock_t		bus_lock_spinlock;
	struct mutex		bus_lock_mutex;

    /********************************
                SPI收发接口
    *********************************/
    int			(*transfer)(struct spi_device *spi,
						struct spi_message *mesg);
    void (*set_cs)(struct spi_device *spi, bool enable);
	int (*transfer_one)(struct spi_controller *ctlr, struct spi_device *spi,
			    struct spi_transfer *transfer);
	void (*handle_err)(struct spi_controller *ctlr,
			   struct spi_message *message);
    /********************************
                SPI DMA
    *********************************/
	bool			(*can_dma)(struct spi_controller *ctlr,
					   struct spi_device *spi,
					   struct spi_transfer *xfer);
    struct dma_chan		*dma_tx;
	struct dma_chan		*dma_rx;
    /********************************
                队列
    *********************************/
    bool				queued;
	struct kthread_worker		*kworker;
	struct kthread_work		pump_messages;
	spinlock_t			queue_lock;
	struct list_head		queue;
	struct spi_message		*cur_msg;
    ...
};

```
2. **struct spi_driver**: SPI外设驱动. 类比platform_driver
```
struct spi_driver {
	const struct spi_device_id *id_table;
	int			(*probe)(struct spi_device *spi);
	int			(*remove)(struct spi_device *spi);
	void			(*shutdown)(struct spi_device *spi);
	struct device_driver	driver;

	ANDROID_KABI_RESERVE(1);
};

/************************************
1. spi_drv_probe: 初始化
2. spi_drv_remove: 

************************************/
int __spi_register_driver(struct module *owner, struct spi_driver *sdrv)
{
	sdrv->driver.owner = owner;
	sdrv->driver.bus = &spi_bus_type;
	if (sdrv->probe)
		sdrv->driver.probe = spi_drv_probe;
	if (sdrv->remove)
		sdrv->driver.remove = spi_drv_remove;
	if (sdrv->shutdown)
		sdrv->driver.shutdown = spi_drv_shutdown;
	return driver_register(&sdrv->driver);
}
EXPORT_SYMBOL_GPL(__spi_register_driver);
```
3. **struct spi_transfer**: SPI数据传输的数据结构
4. **struct spi_message**: spi_transfer链表. 用于一次传输多个spi_transfer
5. **同步传输和异步传输**
    - 同步传输
        - int spi_sync(struct spi_device *spi, struct spi_message *message);
        - 阻塞等待消息被处理完
    - 异步传输
        - int spi_async(struct spi_device *spi, struct spi_message *message);
        - 通过spi_message的complete字段挂接一个回调, 当消息处理完时, 该函数会被调用
6. **struct spi_device**
    - 挂载的具体SPI外设
    - 卸载板载信息数据
7. **struct spi_board_info**
    - 板载信息数据
8. **设备树**
    - 最新的都使用设备, 不再使用struct spi_board_info


## I2C总线设备驱动
    - I2C核心: i2c-core.c
    - I2C总线驱动
        - i2c_adapter: 
        - i2c_algorithm: i2c_adapter的通信方法
        - i2c_
    - I2C设备驱动
        - i2c_driver
        - i2c_client
    - i2c_adapter, i2c_algorithm, i2c_driver, i2c_client
        - i2c_adapter对应物理上的一个适配器, i2c_algorithm对应一套通信方法
        - i2c_adapter需要i2c_algorithm提供的通信函数来控制适配器产生特定的访问周期
        - i2c_driver是一套驱动方法
        - i2c_client对应真实的物理设备
        - i2c_client依附于i2c_adapter, 一个i2c_adapter可连接多个i2c_client设备
    - I2C核心: 不依赖于硬件平台的通用接口
        - 增加/删除i2c_adapter: i2c_add_adapter, i2c_del_adapter
        - 增加/删除i2c_driver: i2c_register_driver, i2c_del_driver
        - I2C传输、发送和接收: i2c_transfer, i2c_master_send, i2c_master_recv
        
1. **struct i2c_adapter**: 一个I2C适配器分配一个设备
    - 
```
struct i2c_adapter {
	struct module *owner;
	unsigned int class;
    /*******************************
                I2C传输函数
    *******************************/
	const struct i2c_algorithm *algo;
	void *algo_data;

	/*******************************
                总线访问锁
    ********************************/
	const struct i2c_lock_operations *lock_ops;
	struct rt_mutex bus_lock;
	struct rt_mutex mux_lock;

    /*******************************
            超时时间, 重试次数
    *******************************/
	int timeout;			/* in jiffies */
	int retries;

    /******************************
            这里的设备模型: 适配器设备
    *******************************/
	struct device dev;		/* the adapter device */


	unsigned long locked_flags;	/* owned by the I2C core */
#define I2C_ALF_IS_SUSPENDED		0
#define I2C_ALF_SUSPEND_REPORTED	1

	int nr;
	char name[48];
	struct completion dev_released;

	struct mutex userspace_clients_lock;
	struct list_head userspace_clients;

	struct i2c_bus_recovery_info *bus_recovery_info;
	const struct i2c_adapter_quirks *quirks;

	struct irq_domain *host_notify_domain;
};
```
2. **struct i2c_algorithm**: I2C主机驱动
    - 最重要的是传输函数指针
```
struct i2c_algorithm {
	
    /********************************
            I2C传输函数指针
    ********************************/
	int (*master_xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs,
			   int num);
	int (*master_xfer_atomic)(struct i2c_adapter *adap,
				   struct i2c_msg *msgs, int num);

    /********************************
            SMBUS传输函数指针
    ********************************/
	int (*smbus_xfer)(struct i2c_adapter *adap, u16 addr,
			  unsigned short flags, char read_write,
			  u8 command, int size, union i2c_smbus_data *data);
	int (*smbus_xfer_atomic)(struct i2c_adapter *adap, u16 addr,
				 unsigned short flags, char read_write,
				 u8 command, int size, union i2c_smbus_data *data);

	/* To determine what the adapter supports */
	u32 (*functionality)(struct i2c_adapter *adap);

    /********************************
                I2C从机
    ********************************/
#if IS_ENABLED(CONFIG_I2C_SLAVE)
	int (*reg_slave)(struct i2c_client *client);
	int (*unreg_slave)(struct i2c_client *client);
#endif
};
```
3. **struct i2c_driver**
    - 
```
struct i2c_driver {
	unsigned int class;

	int (*probe)(struct i2c_client *client, const struct i2c_device_id *id);
	int (*remove)(struct i2c_client *client);

	int (*probe_new)(struct i2c_client *client);
    void (*shutdown)(struct i2c_client *client);

	/***************************
                SMBUS
    ****************************/
	void (*alert)(struct i2c_client *client, enum i2c_alert_protocol protocol,
		      unsigned int data);

	/*****************************
         控制函数, 类似于ioctl
    *****************************/
	int (*command)(struct i2c_client *client, unsigned int cmd, void *arg);

    /*****************************
        通用设备驱动模型
    *****************************/
	struct device_driver driver;

    /*****************************
        用于设备和驱动匹配的id_table
    *****************************/
	const struct i2c_device_id *id_table;

	/*****************************
            设备检测?
    *****************************/
	int (*detect)(struct i2c_client *client, struct i2c_board_info *info);
	
    const unsigned short *address_list;
	struct list_head clients;
};
```
4. **struct i2c_client**:
    - 
```
struct i2c_client {
	unsigned short flags;		/* div., see below		*/
    ...
    /***************************
            I2C地址
    ***************************/
	unsigned short addr;
	char name[I2C_NAME_SIZE];

    /***************************
        每个设备有一个适配器
    ***************************/
	struct i2c_adapter *adapter;

    /***************************
            统一设备模型
    ***************************/
	struct device dev;


	int init_irq;			/* irq set at initialization	*/
	int irq;			/* irq issued by device		*/
	struct list_head detected;

#if IS_ENABLED(CONFIG_I2C_SLAVE)
	i2c_slave_cb_t slave_cb;	/* callback for slave mode	*/
#endif
};
```
5. **struct i2c_msg**: 发送的数据


