

# MMC_eMMC_virtual_disk简介


## 项目
```
代码逻辑
    1. 使用一块内存模式存储卡
    2. 自定义mmc host操作函数, 命令处理函数
    3. 在命令处理过程中实现读写

注意事项
    1. 确保platform_device节点真实存在, 设备树中没有节点, 可以在代码中创建, 并注册到内核中

```

## 问题
1. **struct work_struct与tsklet的区别**


## 使用方法
```
# 编译驱动
make

# 加载驱动
sudo insmod virtual_emmc.ko

# 查看是否成功
dmesg | tail -20
lsblk
ls -la /dev/mmcblk*

# 应该看到类似输出：
# /dev/mmcblk0        (可能是虚拟eMMC设备)

# 分区
sudo fdisk /dev/mmcblk0

# 格式化
sudo mkfs.ext4 /dev/mmcblk0p1

# 挂载
sudo mount /dev/mmcblk0p1 /mnt/virtual_emmc

# 测试性能
dd if=/dev/zero of=/mnt/virtual_emmc/test bs=1M count=100
dd if=/mnt/virtual_emmc/test of=/dev/null bs=1M count=100
```

## 解决方案
```
  首先，MMC子系统中：
  - host：指的是MMC控制器，即硬件上的主机控制器，负责与MMC卡通信的硬件部分。
  - core：MMC核心层，提供通用的MMC功能，包括协议处理、块设备注册等。
  - card：代表具体的MMC卡设备，由core层在检测到卡时创建。

  在Linux内核中，MMC子系统大致分为三层：
  1. 主机控制器驱动（host）：- 负责与硬件交互，实现主机控制器的操作。
  2. 核心层（core）：- 提供MMC/SD协议的核心实现，以及块设备、总线和设备模型的集成。
  3. 卡设备（card）：- 由核心层在检测到卡之后创建，并注册为块设备。

  对于一个虚拟的eMMC驱动（virtual_emmc.c），它可能是在模拟一个MMC主机控制器，然后由核心层来检测虚拟的卡，并创建块设备节点。

  没有生成块设备节点的可能原因：
  1. 驱动没有成功注册为主机控制器。
  2. 核心层没有成功检测到卡（即没有完成卡的初始化过程）。
  3. 块设备注册失败，可能由于设备号分配问题或设备创建失败。

  解决方案：
  1. 检查驱动是否成功注册为主机控制器（检查probe函数是否被调用，以及是否成功调用了mmc_add_host）。
  2. 检查是否模拟了卡的插入和识别过程（例如，是否调用了mmc_detect_change来触发卡检测）。
  3. 检查卡检测过程中是否成功完成了卡的初始化（包括识别、初始化流程），并最终调用mmc_add_card添加卡
```



# 模块简介

## MMC总线
1. **物理信号线**
    - CLK: 主机->卡, 时钟信号。所有数据和命令的传输都以此为基准
    - CMD: 主机->卡->主机, 双向命令/响应线. 发送命令和接收响应
    - RST_n: 主机->卡, 硬件复位信号（eMMC 专用，SD 卡无此引脚）


## MMC协议简介: 协议文档


## MMC协议软件实现: kernel\include\linux\mmc\mmc.h
1. **基本认知**
    - 3个概念
        - **命令**
            - 定义了主机能对卡做什么
        - **状态**
            - 定义了卡当前处于什么阶段
        - **寄存器**
            - 定义了卡的属性和能力
    - MMC驱动
        - 本质: **按照状态机, 发送正确的命令序列, 操作寄存器, 最终完成数据传输**
2. **协议标准文档**
    - 

### 命令: mmc.h
    - bc: 无响应广播命令. 主机发出后, 所有卡都执行, 不回应
    - bcr: 有响应广播命令. 所有卡都执行, 并同时回应, 主机需处理冲突
    - ac: 点对点寻址命令. 命令参数中带有RCA, 只有被选中的卡才执行并回应
    - adtc: 带数据传输的点对点命令. 命令、响应、数据三个阶段都有
    - R1/R1b/R2/R3...: 响应类型. 定义了卡返回的48位或136位响应的格式
1. **卡识别与初始化流程**: 选卡, 分配地址, 获取卡参数
    - MMC_GO_IDLE_STATE：上电复位，让卡进入 Idle 状态
    - MMC_SEND_OP_COND：主机发送自己的电压范围，卡回应是否支持。这是电压协商的关键
    - MMC_ALL_SEND_CID：获取卡的唯一 ID。所有卡同时回应，主机通过检测冲突来发现多张卡
    - MMC_SET_RELATIVE_ADDR：主机给卡分配一个短地址。此后，点对点通信就靠它
    - MMC_SELECT_CARD：激活这张卡，让其进入数据传输状态
    - MMC_SEND_CSD：获取卡的特性数据寄存器，包含容量、速度等级、块大小等关键信息
2. **数据传输命令**
    - MMC_READ_SINGLE_BLOCK / MMC_WRITE_BLOCK：单块读写。地址和长度由命令参数指定
    - MMC_READ_MULTIPLE_BLOCK / MMC_WRITE_MULTIPLE_BLOCK：多块读写。必须先通过 MMC_SET_BLOCK_COUNT 设置块数量，然后一次命令传输所有数据，效率远高于单块循环
3. **高级功能命令**: Class 5, 6, 7, 8, 9, 11
    - Class 5 (Erase)：MMC_ERASE_GROUP_START/END 和 MMC_ERASE 构成三段式擦除操作，这是 eMMC 实现 TRIM/Discard 功能的硬件基础
    - Class 7 (Lock/Unlock)：MMC_LOCK_UNLOCK 用于卡级锁定，保护数据安全
    - Class 11 (Command Queue)：这是 eMMC 5.1 引入的关键性能特性
        - MMC_QUE_TASK_PARAMS、MMC_EXECUTE_READ_TASK 等命令允许卡内部维护一个任务队列，乱序执行读写，极大提升随机 I/O 性能。这就是硬件层面的命令队列
    
### 状态与识别: mmc.h
    - 发出命令后, 卡会响应
    - R1是最常见的48位响应, 其高16位就是卡的状态字
1. **状态位**
    - 状态位
        - 注释中的 e (error), s (status), r (response), x (execution) 和 a (auto clear), b (clear by next cmd), c (clear by read) 是理解状态位生命周期的关键

### 配置空间: CSD与EXT_CSD寄存器
    - 驱动通过读取这些寄存器来获取卡的能力
1. **CSD(Card Specific Data)**
    - 通过MMC_SEND_CSD命令获取
2. **EXT_CSD(Extended CSD)**
    - eMMC 4.x+ 的核心
    - 通过 MMC_SEND_EXT_CSD 命令获取
    - 是一个 512 字节的配置空间，包含了所有高级特性


## MMC驱动

### 核心数据结构
1. **struct mmc_blk_data**: 用于注册gendisk设备, 位于mmc\card\block.c
```
struct mmc_blk_data {
	spinlock_t	lock;
	struct gendisk	*disk;
	struct mmc_queue queue;
	struct list_head part;
    ...
	unsigned int	part_curr;
	struct device_attribute force_ro;
	struct device_attribute power_ro_lock;
	int	area_type;
};
```
2. **mmc/core/bus.c, host.c, core.c**
    - struct mmc_host: 抽象一个主机控制器，包含其操作函数集ops、时钟、电压、状态等信息
    - struct mmc_card: 抽象一张插入的卡，存储其CID、CSD、RCA、类型（MMC/SD/SDIO）等信息
    - struct mmc_request (mrq): 代表一个完整的I/O请求，可以包含一个命令和多个数据请求
    - struct mmc_command (cmd): 代表一个具体的MMC协议命令
    - struct mmc_data: 代表与命令相关的数据传输（读/写）

3. **struct mmc_host_ops**: host.h
```
struct mmc_host_ops {

	void	(*post_req)(struct mmc_host *host, struct mmc_request *req,
			    int err);
	void	(*pre_req)(struct mmc_host *host, struct mmc_request *req,
			   bool is_first_req);
	void	(*request)(struct mmc_host *host, struct mmc_request *req);


	void	(*set_ios)(struct mmc_host *host, struct mmc_ios *ios);
	int	(*get_ro)(struct mmc_host *host);
	int	(*get_cd)(struct mmc_host *host);

	void	(*enable_sdio_irq)(struct mmc_host *host, int enable);


	void	(*init_card)(struct mmc_host *host, struct mmc_card *card);

	int	(*start_signal_voltage_switch)(struct mmc_host *host, struct mmc_ios *ios);


	int	(*card_busy)(struct mmc_host *host);
	int     (*set_sdio_status)(struct mmc_host *host, int val);


	int	(*execute_tuning)(struct mmc_host *host, u32 opcode);


	int	(*prepare_hs400_tuning)(struct mmc_host *host, struct mmc_ios *ios);

	void	(*hs400_enhanced_strobe)(struct mmc_host *host, struct mmc_ios *ios);
	int	(*select_drive_strength)(struct mmc_card *card,
					 unsigned int max_dtr, int host_drv,
					 int card_drv, int *drv_type);
	void	(*hw_reset)(struct mmc_host *host);
	void	(*card_event)(struct mmc_host *host);


	int	(*multi_io_quirk)(struct mmc_card *card,
				  unsigned int direction, int blk_size);
};
```



