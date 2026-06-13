
# led_platform_device简介

## 项目
```
基本逻辑: 创建一个device实体, 创建一个driver实体, 创建一个操作device的实体, 将它们关联起来

项目通过led驱动接入，介绍platform device模型
通过项目了解: platform_bus, platform_device, platform_driver, resource概念

代码逻辑:
	1. 创建struct platform_data, 承载硬件信息: GPIO地址, 驱动LED的GPIO引脚
	2. 创建struct platform_driver
		- 创建drvier, 实现init和exit接口
		- 实现probe, remove接口
			- probe: 实现硬件初始化
				- 创建一个字符设备
				- 创建一个字符设备操作函数
				- 注册字符设备
				- 完成硬件寄存器空间到字符设备虚拟空间的映射
				- 完成设备节点创建和绑定
			- remove
				- 资源回收
	3. 最终执行led访问操作的是字符设备操作函数



1. struct device的作用是承载资源信息, 包括硬件信息 + 软件信息
	- platform_data: 可以用来承载硬件信息
	- driver_data: 可以用来承载驱动信息(软件信息)
2. struct device_driver:的作用是初始化设备
	- 首先需要获取设备的硬件资源信息
	- 然后为操作这些硬件信息提供软件资源
	- 并把操作接口作为驱动信息赋值给driver_data
3. 不同设备的驱动
	- 设备类型
		- 字符设备
		- 块设备
		- 网络设备
	- 不同设备类型的操作函数不同, 需要在不同驱动中实现
```

## 问题
1. **设备树节点**
	- 内核在解析设备树的, 会自动为每个包含compatible属性的节点创建platform_device
	- of_platform_bus_create
		- drivers/of/platform.c
2. **设备节点与device**
	- 在内核解析设备树时, 一些devcie是内核自动创建的
		- Platform设备: 设备树扫描自动创建
		- I2C设备: I2C总线扫描创建
		- SPI设备: SPI总线扫描创建
		- USB设备: USB总线枚举创建
		- PCI设备: PCI总线枚举创建
	- 有些设备需要手动创建
		- 网络设备: alloc_etherdev
		- 块设备: alloc_disk
		- 字符设备: register_chrdev






# 模块简介

## platform device: bus, device, driver, resource
1. **platform bus**: 平台总线
    - 一种虚拟的总线
    - 为那些不需要总线的设备, 进行Linux驱动开发时, 设计的一种虚拟总线模型
        - LED, RTC时钟, 蜂鸣器, 按键等这些设备没有相应的物理总线
        - I2C, SPI, USB等常见物理总线, 在Linux内核中会自动创建与之相应的驱动总线

2. **struct platform_device**: 平台设备
```
struct platform_device {
	const char	*name;
	int		id;
	bool		id_auto;

	struct device	dev;                    //统一设备模型

	u64		platform_dma_mask;
	struct device_dma_parameters dma_parms; //DMA参数
	u32		num_resources;
	struct resource	*resource;              //资源

	const struct platform_device_id	*id_entry;

	char *driver_override;  //用于match, driver_set_override()去清空

	/* MFD cell pointer */
	struct mfd_cell *mfd_cell;

	/* arch specific additions */
	struct pdev_archdata	archdata;

	ANDROID_KABI_RESERVE(1);
	ANDROID_KABI_RESERVE(2);
};
```

3. **struct platform_driver**: 平台设备驱动
```
struct platform_driver {

    /*除了基本的驱动, 还特化了平台驱动所需要的接口*/
	int (*probe)(struct platform_device *);
	int (*remove)(struct platform_device *);
	void (*shutdown)(struct platform_device *);
	int (*suspend)(struct platform_device *, pm_message_t state);
	int (*resume)(struct platform_device *);

	struct device_driver driver;        //统一驱动模型

	const struct platform_device_id *id_table;
	bool prevent_deferred_probe;

	ANDROID_KABI_RESERVE(1);
};
```

4. **struct resource**: 资源 = 硬件信息 + 软件信息
```
struct resource {
	resource_size_t start;  //资源起始地址
	resource_size_t end;    //资源结束地址
	const char *name;       //资源的名字, 可以是NULL
	unsigned long flags;    //资源类型: IORESOURCE_IO(IO地址空间), IORESOURCE_MEM(外设可直接寻址的地址空间), IORESOURCE_IRQ(中断), IORESOURCE_DMA(DMA通道)
	unsigned long desc;
	struct resource *parent, *sibling, *child;
};
```

## struct resource: 资源详解
```
struct resource {
	resource_size_t start;  //资源起始地址
	resource_size_t end;    //资源结束地址
	const char *name;       //资源的名字, 可以是NULL
	unsigned long flags;    //资源类型: IORESOURCE_IO(IO地址空间), IORESOURCE_MEM(外设可直接寻址的地址空间), IORESOURCE_IRQ(中断), IORESOURCE_DMA(DMA通道)
	unsigned long desc;
	struct resource *parent, *sibling, *child;
};
```
1. **资源类型**
    - IORESOURCE_IO
    - IORESOURCE_MEM
    - IORESOURCE_IRQ
    - IORESOURCE_DMA
1. **定义MEM资源**
    - DEFINE_RES_MEM(_start, _size)
    - DEFINE_RES_MEM_NAMED(_start, _size, _name)
2. **定义IO资源**
    - DEFINE_RES_IO(_start, _size)
    - DEFINE_RES_IO_NAMED(_start, _size, _name)
3. **定义IRQ资源**
    - DEFINE_RES_IRQ(_irq): 中断号. size默认是1
    - DEFINE_RES_IRQ_NAMED(_irq, _name)
4. **定义DMA资源**
    - DEFINE_RES_DMA(_dma): DMA通道号, 资源默认是1
    - DEFINE_RES_DMA_NAMED(_dma, _name)

## 平台总线match逻辑
```
1. driver_override 强制匹配
   ↓ 失败
2. OF (Device Tree) 风格匹配
   ↓ 失败
3. ACPI 风格匹配
   ↓ 失败
4. ID table 匹配
   ↓ 失败
5. 名称直接匹配（fallback）
```




