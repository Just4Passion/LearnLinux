

# RAM_Block_virtual_disk简介

## 项目
```


```

## 使用
```
# 创建文件系统
sudo mkfs.ext4 /dev/ramdisk

# 挂载设备
sudo mkdir /mnt/ramdisk
sudo mount /dev/ramdisk /mnt/ramdisk

# 使用设备
echo "Hello RAM Disk" | sudo tee /mnt/ramdisk/test.txt
cat /mnt/ramdisk/test.txt

# 卸载设备
sudo umount /mnt/ramdisk
```



## 接口
    - 块设备
    - 请求队列
    - 磁盘分区
1. **register_blkdev**: 注册块设备
    - 
2. **struct request_queue**: 请求队列
    - blk_mq_init_queue(): 初始化队列
    - 队列交换数据单元
        - blk_queue_logical_block_size(): 设置逻辑块大小
        - blk_queue_physical_block_size(): 设置物理块大小
3. **struct blk_mq_tag_set**: 多队列标签
    - 是什么
        - 内核块层（Block Layer）多队列（blk-mq）框架中的核心数据结构
        - 它定义了一组请求标签（tag）的集合，用于管理和分配块设备请求的标识符
    - 作用
        - 标签池管理: 维护一个全局标签, 每个标签对应一个struct request
        - 多队列映射
            - tag_set定义了硬件队列(hw_queue)和软件队列(ctx)的映射关系
            - 决定了请求如何分发到不同的硬件队列
        - 资源分配与限制
            - 通过queue_depth控制设备的最大并发请求数, 通过nr_hw_queue定义硬件队列数量
        - 驱动与块接口层
            - 块设备驱动通过填充并注册 tag_set 来初始化 blk-mq，它是驱动与通用块层之间的关键桥梁
    - 成员
        - struct blk_mq_queue_map map[HCTX_MAX_TYPES]
            - 定义CPU到硬件队列的映射关系
        - struct blk_mq_ops *ops
            - 指向驱动实现的 blk-mq 操作函数集
            - 包括 queue_rq（提交请求）、init_request（初始化请求）、exit_request（释放请求）、complete（完成请求）等回调
        - nr_hw_queues
            - 硬件队列的总数. 通常等于设备支持的并行通道数（如 NVMe 的队列数）
        - nr_maps
            - 映射表的数量，通常等于 HCTX_MAX_TYPES，但可以更少（例如不支持读写分离时设为 1）
        - queue_depth
            - 每个硬件队列的最大请求数（标签数）。总标签数 = nr_hw_queues × queue_depth。它决定了设备能同时处理的 I/O 请求数量
        - flags
            - 控制标签集行为的标志位
            - BLK_MQ_F_SHOULD_MERGE：启用请求合并
        - 
    - 接口
        - blk_mq_alloc_tag_set(): 分配标签
4. **add_disk**: 添加磁盘到系统中
    - 

