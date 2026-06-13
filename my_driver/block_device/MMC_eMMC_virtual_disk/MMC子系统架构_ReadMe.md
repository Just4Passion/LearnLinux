
# MMC子系统架构
    - 每个块设备有一个队列
    - 一个存储卡会被拆分成多个分区, 每个分区由一个块设备负责
    - 三种类型
        - SD卡：忽略CDM52
        - eMMC: 忽略CDM52
        - SDIO: 总线. 需要处理CMD52

1. **MMC子系统作用**
    - 负责管理MMC、SD、SDIO卡的识别、初始化和数据传输
    - 采用经典的总线-设备-驱动模型

2. **核心层次**
    - 块设备层
    - MMC核心层
    - 主机控制器驱动层

## 块设备层: block.c
    - mmc/card/block.c
    - 实现请求队列、I/O调度，将块请求转为MMC命令

1. **核心数据结构**
    - struct mmc_blk_data
        - struct gendisk *disk;
        - struct mmc_queue queue;
        - spinlock_t lock;

2. **调用关系**
    - 基本逻辑
        - 注册mmc块设备(major=179) -> 注册mmc驱动
        - probe: 入参是struct mmc_card, 也就是存储卡
            - 给卡分配块设备disk -> 给卡的每个分区分配一个分区块设备 -> 添加disk 
    - 关键变量
        - struct block_device_operations mmc_bdops: 块设备操作函数
        - struct mmc_dirver mmc_driver: mmc设备驱动
    - 注册驱动
        - mmc_blk_init: register_blkdev -> mmc_register_driver
    - 初始化块设备: 
        - mmc_blk_probe: **mmc_blk_alloc** -> mmc_blk_alloc_parts -> **mmc_add_disk**
    - 创建块设备
        - mmc_blk_alloc: mmc_blk_alloc_req 
        - mmc_blk_alloc_req
            - 初始化队列: mmc_init_queue(&md->queue, card, &md->lock, subname)
            - 内部申请disk, 赋值disk->major=179, disk->fops=&mmc_bdops



## 核心层
    - mmc/core/
    - 总线冲裁: bus.c
    - 卡识别与初始化状态机
    - 协议命令封装
    - 电源管理
1. **核心数据结构**
    - struct mmc_host: 抽象一个主机控制器，包含其操作函数集ops、时钟、电压、状态等信息
    - struct mmc_card: 抽象一张插入的卡，存储其CID、CSD、RCA、类型（MMC/SD/SDIO）等信息
    - struct mmc_request (mrq): 代表一个完整的I/O请求，可以包含一个命令和多个数据请求
    - struct mmc_command (cmd): 代表一个具体的MMC协议命令
    - struct mmc_data: 代表与命令相关的数据传输（读/写）
2. **核心功能**
    - 卡检测: 通过**mmc_rescan**工作队列，定期或由中断触发，扫描卡槽状态
    - 状态机: 驱动卡的初始化过程，从Idle到Ready，再到Ident，最后到Stand-by和Transfer状态
    - 协议命令封装: 提供mmc_wait_for_cmd, mmc_wait_for_req等便捷函数，供上层调用
    - 总线管理: mmc_bus结构体定义了卡和驱动之间的匹配规则
3. **卡检测**: mmc_rescan
    - mmc_rescan -> mmc_rescan_try_freq -> mmc_power_up -> mmc_attach_sdio(sdio.c)/mmc_attach_sd(sd.c)/mmc_attach_mmc(mmc.c) -> mmc_add_card(bus.c) -> device_add
        - SDIO发送CMD52去复位card; SD/eMMC不需要处理CMD52
        - **device_add触发MMC总线上的驱动匹配**

## 主机控制器驱动层
    - mmc/host/
    - 实现struct mmc_host_ops中的硬件操作函数
    - 处理DMA、中断，与控制器寄存器交互
1. **核心数据结构**
    - struct mmc_host_ops，这是一组函数指针，由具体驱动实现
        - 直接与硬件打交道的部分
        - 它负责将mmc_request中的命令和数据，写入主机控制器的寄存器，配置DMA，处理传输完成中断，并最终通知核心层




