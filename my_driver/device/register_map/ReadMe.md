
# register_map简介

## 项目
```
学习如何通过regmap访问寄存器
1. struct regmap_config: 初始化寄存器配置, 并编写号它的访问操作函数
2. struct regmap: 将config与具体的regmap进行绑定
3. regmap_write, regmap_bulk_write, regmap_read, regmap_bulk_read
```




# 模块简介

## Regmap简介
    - 本质是中间层, 向上提供统一的访问寄存器的接口, 实际操作接口依然要通过I/O访问的形式访问寄存器, 或者通过总线控制器提供的接口访问总线
1. **概念**
    - 寄存器映射
        - 将不同外设(SPI, I2C, MMIO等)寄存器的操作, 封装成统一的API接口
        - 让开发者无需关心底层通信协议的细节, 只需要调用统一的函数接口, 即可完成寄存器的读、写、批量操作
    - 特点
        - 内置缓存机制、锁机制和错误处理机制
    - 优势
        - 统一接口, 简化开发
        - 内置缓存, 提升效率
        - 原子操作, 保证安全: 内置自旋锁/互斥锁机制
        - 支持批量操作
        - 可扩展性强
2. **struct regmap_config**：寄存器的配置, 特性
    - 寄存器地址位宽
    - 寄存器数据位宽
    - 缓存策略
```
struct regmap_config {
	const char *name;

	int reg_bits;
	int reg_stride;
	int pad_bits;
	int val_bits;

	bool (*writeable_reg)(struct device *dev, unsigned int reg);
	bool (*readable_reg)(struct device *dev, unsigned int reg);
	bool (*volatile_reg)(struct device *dev, unsigned int reg);
	bool (*precious_reg)(struct device *dev, unsigned int reg);
	bool (*writeable_noinc_reg)(struct device *dev, unsigned int reg);
	bool (*readable_noinc_reg)(struct device *dev, unsigned int reg);

	bool disable_locking;
	regmap_lock lock;
	regmap_unlock unlock;
	void *lock_arg;

	int (*reg_read)(void *context, unsigned int reg, unsigned int *val);
	int (*reg_write)(void *context, unsigned int reg, unsigned int val);

	bool fast_io;

	unsigned int max_register;
	const struct regmap_access_table *wr_table;
	const struct regmap_access_table *rd_table;
	const struct regmap_access_table *volatile_table;
	const struct regmap_access_table *precious_table;
	const struct regmap_access_table *wr_noinc_table;
	const struct regmap_access_table *rd_noinc_table;
	const struct reg_default *reg_defaults;
	unsigned int num_reg_defaults;
	enum regcache_type cache_type;
	const void *reg_defaults_raw;
	unsigned int num_reg_defaults_raw;

	unsigned long read_flag_mask;
	unsigned long write_flag_mask;
	bool zero_flag_mask;

	bool use_single_read;
	bool use_single_write;
	bool can_multi_write;

	enum regmap_endian reg_format_endian;
	enum regmap_endian val_format_endian;

	const struct regmap_range_cfg *ranges;
	unsigned int num_ranges;

	bool use_hwlock;
	unsigned int hwlock_id;
	unsigned int hwlock_mode;

	bool can_sleep;

	ANDROID_KABI_RESERVE(1);
};
```
3. **struct regmap**: 寄存器实体. 封装了寄存器的缓存、锁、配置、底层通信接口. 所有寄存器操作都通过其完成
```
struct regmap {
    /* 并发保护机制：支持互斥锁和自旋锁，保证寄存器操作原子性 */
    union {
        struct mutex mutex;               /* 互斥锁，用于进程上下文并发保护 */
        struct {
            spinlock_t spinlock;          /* 自旋锁，用于中断上下文并发保护 */
            unsigned long spinlock_flags; /* 自旋锁标志位 */
        };
    };
    /* 自定义锁/解锁函数，用于替换Regmap默认锁机制 */
    regmap_lock lock;
    regmap_unlock unlock;
    void *lock_arg;                 /* 传递给锁/解锁函数的上下文参数 */

    /* 设备及核心操作相关 */
    struct device *dev;             /* 关联的设备结构体，用于I/O操作关联 */
    void *work_buf;                 /* 用于格式化I/O操作的临时缓存区 */
    const struct regmap_bus *bus;   /* 底层通信总线接口，屏蔽SPI/I2C/MMIO差异 */
    void *bus_context;              /* 总线上下文指针，如SPI/I2C设备指针 */
    const char *name;               /* Regmap实例名称 */

    /* 核心寄存器操作函数，可自定义实现特殊访问逻辑 */
    int (*reg_read)(void *context, unsigned int reg, unsigned int *val);
    int (*reg_write)(void *context, unsigned int reg, unsigned int val);
    int (*reg_update_bits)(void *context, unsigned int reg,
                        unsigned int mask, unsigned int val);

    /* 寄存器范围及访问控制 */
    unsigned int max_register;      /* 最大寄存器地址，用于边界检查 */
    bool (*writeable_reg)(struct device *dev, unsigned int reg); /* 自定义可写判断 */
    bool (*readable_reg)(struct device *dev, unsigned int reg);  /* 自定义可读判断 */
    const struct regmap_access_table *wr_table; /* 可写寄存器表 */
    const struct regmap_access_table *rd_table; /* 可读寄存器表 */

    /* 读写标志及地址配置 */
    unsigned long read_flag_mask;   /* 读操作附加标志掩码 */
    unsigned long write_flag_mask;  /* 写操作附加标志掩码 */
    int reg_shift;                  /* 寄存器地址移位位数 */
    int reg_stride;                 /* 寄存器地址步长 */

    /* 缓存相关，Regmap核心优势相关成员 */
    enum regcache_type cache_type;      /* 缓存类型，如写透、回写 */
    struct reg_default *reg_defaults;   /* 寄存器默认值数组 */
    const void *reg_defaults_raw;       /* 原始格式寄存器默认值 */
    void *cache;                        /* 寄存器缓存存储区 */
    bool cache_dirty;                   /* 缓存是否脏标记 */
    unsigned int num_reg_defaults;      /* 寄存器默认值个数 */

    /* 批量操作控制 */
    bool can_multi_write;           /* 是否支持批量写操作 */
    bool use_single_read;           /* 是否将批量读转为单次读 */
    bool use_single_write;          /* 是否将批量写转为单次写 */

    size_t max_raw_read;            /* 最大原始读操作长度限制 */
    size_t max_raw_write;           /* 最大原始写操作长度限制 */

    /* 省略部分成员 */
};
```
4. **struct regmap_bus**: 寄存器总线. 封装了对应总线的寄存器读写实现. 屏蔽了不同总线的底层差异
```
struct regmap_bus {
	bool fast_io;
	regmap_hw_write write;
	regmap_hw_gather_write gather_write;
	regmap_hw_async_write async_write;
	regmap_hw_reg_write reg_write;
	regmap_hw_reg_update_bits reg_update_bits;
	regmap_hw_read read;
	regmap_hw_reg_read reg_read;
	regmap_hw_free_context free_context;
	regmap_hw_async_alloc async_alloc;
	u8 read_flag_mask;
	enum regmap_endian reg_format_endian_default;
	enum regmap_endian val_format_endian_default;
	size_t max_raw_read;
	size_t max_raw_write;

	ANDROID_KABI_RESERVE(1);
};
```


## OLED控制芯片: SSD1306



