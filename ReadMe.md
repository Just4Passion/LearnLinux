# Linux Kernel Driver Learning Project (Linux_VM)

> **Author**: DYWorker001  
> **Platform**: Ubuntu 22.04.5 LTS (Linux Kernel 5.15.189)  
> **Architecture**: ARM64 (RK3568 / LubanCat2)  
> **Purpose**: Linux 内核驱动开发学习与实践项目

---

## 项目概述

本项目是一个系统的 **Linux 内核驱动开发学习项目**，涵盖字符设备驱动、平台设备驱动、设备树、块设备驱动等核心主题。项目从最基础的 Hello World 内核模块开始，逐步深入到复杂的块设备驱动实现，每一部分都配有详细的注释和使用说明。

### 整体架构

```
Linux_VM/
├── Doc/                                    # 文档资源
├── my_driver/                              # 内核驱动源码
│   ├── helloworld/                         # Hello World 内核模块
│   ├── device/                             # 字符设备系列驱动
│   │   ├── char_device/                    # 基础字符设备驱动
│   │   ├── device_module/                  # 总线/设备/驱动模型
│   │   ├── device_tree/                    # 设备树操作函数
│   │   ├── globalfifo/                     # 全局FIFO字符设备（阻塞IO）
│   │   ├── globalmem/                      # 全局内存字符设备
│   │   ├── led_char_device/                # LED字符设备（MMIO映射）
│   │   ├── led_platform_device/            # LED平台设备模型
│   │   └── second_soft_timer/              # 秒软件定时器设备
│   └── block_device/                       # 块设备系列驱动
│       ├── MMC_eMMC_virtual_disk/          # 虚拟eMMC设备驱动
│       └── RAM_Backed_virtual_disk/        # 内存模拟块设备
```

---

## 目录详细介绍

### Doc/

**功能**：项目文档目录

| 文件 | 说明 |
|------|------|
| `ReadMe.md` | 记录了使用的 Ubuntu ISO 镜像版本: `ubuntu-22.04.5-live-server-amd64.iso` |

**关键知识点**：
- Ubuntu Server 22.04.5 LTS 作为实验环境
- Linux 内核版本 5.15.189

---

### my_driver/helloworld/

**功能**：最基础的 Linux 内核模块示例，用于验证内核模块开发环境

**关键知识点**：
- `module_init()` / `module_exit()` — 模块入口/出口宏
- `MODULE_LICENSE()` / `MODULE_AUTHOR()` — 模块信息声明
- `printk()` — 内核打印函数（内核态 `printf`）
- 内核模块的编译框架 (`obj-m`, `make -C /lib/modules/$(KVERS)/build`)

---

### my_driver/device/char_device/

**功能**：完整的字符设备驱动实现，演示字符设备的完整生命周期

**源文件**：

| 文件 | 说明 |
|------|------|
| `char_device.c` | 字符设备驱动核心代码 |
| `test_demo.c` | 用户空间测试程序（打开 → 写入 → 读取 → 关闭） |

**关键知识点**：
1. **设备号管理**
   - `alloc_chrdev_region()` — 动态分配设备号
   - `MAJOR()` / `MINOR()` — 提取主/次设备号
   - `MKDEV()` — 组合设备号

2. **字符设备注册流程**
   - `cdev_init()` — 初始化 cdev 结构体并绑定 `file_operations`
   - `cdev_add()` — 向系统注册字符设备
   - `class_create()` — 创建设备类（在 `/sys/class/` 下可见）
   - `device_create()` — 创建设备节点（自动生成 `/dev/` 节点）

3. **文件操作接口**
   - `open()` / `release()` — 打开/释放设备
   - `read()` / `write()` — 读写操作（使用 `copy_to_user` / `copy_from_user`）
   - `unlocked_ioctl()` / `compat_ioctl()` — 设备控制命令

4. **内存安全**
   - `copy_from_user()` / `copy_to_user()` — 用户空间与内核空间数据拷贝
   - 文件偏移管理 (`*ppos`)

---

### my_driver/device/device_module/

**功能**：实现自定义总线模型，演示 Linux 设备驱动模型的 **Bus-Device-Driver** 分离架构

**源文件**：

| 文件 | 说明 |
|------|------|
| `xbus.c` | 自定义 "xbus" 总线实现 |
| `xdev.c` | 挂载在 xbus 上的设备 "xdev" |
| `xdrv.c` | 挂载在 xbus 上的驱动 "xdrv" |
| `test_demo.c` | 用户空间测试程序（交互式 sysfs 属性测试） |

**关键知识点**：
1. **总线模型（Bus Model）**
   - `struct bus_type` — 总线结构体（name, match, probe, remove 等回调）
   - `bus_register()` / `bus_unregister()` — 总线注册/注销
   - `bus_create_file()` / `bus_remove_file()` — 总线属性文件创建/删除

2. **设备（Device）**
   - `struct device` — 设备结构体（init_name, bus, release）
   - `device_register()` / `device_unregister()` — 设备注册/注销
   - `device_create_file()` — 在 sysfs 中创建设备属性文件
   - `DEVICE_ATTR()` — 定义设备属性（`_show` / `_store` 函数）

3. **驱动（Driver）**
   - `struct device_driver` — 驱动结构体（name, bus, probe, remove）
   - `driver_register()` / `driver_unregister()` — 驱动注册/注销
   - `DRIVER_ATTR_RO()` — 定义只读驱动属性

4. **匹配机制（Match）**
   - `match()` 回调通过 `strncmp(dev_name(dev), drv->name, ...)` 实现设备和驱动的匹配
   - 设备名 "xdev" 和驱动名 "xdev" 一致时匹配成功
   - 匹配成功后自动调用驱动的 `probe()` 函数

5. **sysfs 交互**
   - 总线属性: `/sys/bus/xbus/xbus_test`
   - 设备属性: `/sys/devices/xdev/xdev_id`
   - 驱动属性: `/sys/bus/xbus/drivers/xdev/drvname`

---

### my_driver/device/device_tree/

**功能**：演示设备树（Device Tree）的节点访问和属性读取操作

**源文件**：

| 文件 | 说明 |
|------|------|
| `device_tree_func.c` | 设备树操作函数驱动代码 |
| `rk3568-lubancat2.dts` | LubanCat2 开发板的完整设备树源文件 |
| `rk3568.dtsi` | RK3568 SoC 的设备树头文件（3622行） |

**关键知识点**：
1. **设备树基础**
   - DTS (Device Tree Source) / DTB (Device Tree Binary) / DTC (Device Tree Compiler)
   - 节点（node）：`node-name@unit-address`
   - 标签（label）：`label: node-name@unit-address`
   - 路径（path）：从根节点到目标节点的完整路径
   - 属性（property）：compatible, reg, status, #address-cells, #size-cells 等

2. **OF（Open Firmware）操作函数**
   - `of_find_node_by_path()` — 通过路径查找节点
   - `of_get_next_child()` — 获取下一个子节点
   - `of_find_property()` — 查找节点属性
   - `of_property_read_u32_array()` — 读取 u32 数组类型属性
   - `of_match_ptr()` — 匹配表注册宏

3. **Platform Driver 与设备树结合**
   - `struct of_device_id` — 匹配表，与设备树的 compatible 属性匹配
   - `module_platform_driver()` — 简化平台驱动注册宏

4. **RK3568 SoC 硬件资源**
   - GPIO 控制器基地址: `0xfdd60000`
   - 时钟、中断控制器、I2C、UART、PWM、USB、GMAC 等外设

---

### my_driver/device/globalmem/

**功能**：全局内存字符设备，演示共享内存、互斥访问、ioctl 控制命令

**源文件**：

| 文件 | 说明 |
|------|------|
| `globalmem_char_device.c` | 全局内存字符设备驱动 |

**关键知识点**：
1. **内存管理**
   - `kzalloc()` — 内核内存动态分配（清零）
   - `GLOBALMEM_SIZE = 0x1000` (4KB) 共享内存缓冲区

2. **互斥访问**
   - `struct mutex` / `mutex_lock()` / `mutex_unlock()` — 互斥锁保护共享资源

3. **文件定位**
   - `llseek()` — 文件读写指针定位接口
   - `SEEK_SET` / `SEEK_CUR` / `SEEK_END` — 三种定位方式

4. **ioctl 命令**
   - `_IO(type, nr)` — 定义无参命令（`MEM_CLEAR` 清零内存）
   - `unlocked_ioctl()` — 处理 ioctl 调用

5. **模块参数**
   - `module_param()` — 允许在加载模块时传入参数（如 `insmod xxx.ko globalmem_major=230`）

---

### my_driver/device/globalfifo/

**功能**：全局 FIFO 字符设备，演示**阻塞/非阻塞 IO**、**等待队列**、**轮询（poll）**、**异步通知**等高级 IO 模型

**源文件**：

| 文件 | 说明 |
|------|------|
| `globalfifo_char_device.c` | 全局 FIFO 字符设备驱动核心（468行） |
| `test_demo.c` | 用户空间测试程序（支持 select/epoll/signal/signalio 测试） |

**关键知识点**：
1. **等待队列（Wait Queue）**
   - `wait_queue_head_t` — 等待队列头
   - `DECLARE_WAITQUEUE()` — 声明等待队列元素
   - `add_wait_queue()` / `remove_wait_queue()` — 添加/移除等待队列
   - `__set_current_state(TASK_INTERRUPTIBLE)` — 设置进程状态
   - `schedule()` — 触发进程调度（进程在此睡眠）
   - `wake_up_interruptible()` — 唤醒等待队列中的进程

2. **阻塞 vs 非阻塞**
   - 阻塞模式：`O_NONBLOCK` 未设置，条件不满足时进程挂起
   - 非阻塞模式：条件不满足时立即返回 `-EAGAIN`

3. **轮询机制（Poll）**
   - `poll_wait()` — 将进程注册到驱动的等待队列
   - `POLLIN | POLLRDNORM` — 可读标志
   - `POLLOUT | POLLWRNORM` — 可写标志
   - 用户空间通过 `select()` / `epoll()` 调用触发

4. **异步通知（fasync）**
   - `struct fasync_struct` — 异步通知队列结构
   - `fasync_helper()` — 管理异步通知队列
   - `kill_fasync()` — 发送 SIGIO 信号给用户进程
   - 用户空间：`fcntl(F_SETOWN)` + `fcntl(F_SETFL, FASYNC)` + `signal(SIGIO)`

5. **用户空间 IO 多路复用**
   - `select()` — 传统 IO 多路复用（fd_set, FD_ZERO, FD_SET, FD_ISSET）
   - `epoll` — 高性能 IO 多路复用（epoll_create, epoll_ctl, epoll_wait）
   - 信号驱动 IO（SIGIO 信号处理）

6. **进程调度与状态**
   - `TASK_RUNNING` / `TASK_INTERRUPTIBLE` / `TASK_UNINTERRUPTIBLE`
   - `signal_pending()` — 检查是否有信号挂起
   - `set_current_state()` — 设置当前进程状态

---

### my_driver/device/led_char_device/

**功能**：LED 字符设备驱动，演示**物理地址映射**和 **GPIO 寄存器操作**

**源文件**：

| 文件 | 说明 |
|------|------|
| `led_char_device.c` | LED 字符设备驱动代码 |
| `test_demo.c` | 用户空间测试程序（本目录无，复用 led_platform_device 的 test_demo） |

**关键知识点**：
1. **MMU 与地址映射**
   - MMU 使能后，外设物理地址不能直接访问
   - `ioremap()` — 将物理地址映射到内核虚拟地址空间
   - `iounmap()` — 解除映射

2. **IO 内存访问**
   - `ioread32()` — 从映射地址读取 32 位值
   - `iowrite32()` — 向映射地址写入 32 位值
   - 这些函数提供内存屏障、总线宽度对齐、字节序处理等保障

3. **GPIO 寄存器操作**
   - GPIO 数据寄存器（DR）：设置/读取引脚电平
   - GPIO 方向寄存器（DDR）：设置引脚为输入或输出
   - 高 16 位为使能位，低 16 位为数据位

---

### my_driver/device/led_platform_device/

**功能**：LED 平台设备驱动，演示 **platform device 模型**（设备与驱动分离）

**源文件**：

| 文件 | 说明 |
|------|------|
| `led_platform_dev.c` | 平台设备注册 |
| `led_platform_drv.c` | 平台驱动注册（含字符设备） |
| `test_demo.c` | 用户空间测试程序（开/关灯测试） |

**关键知识点**：

1. **Platform Device 模型**
   - `struct platform_device` — 平台设备（name, id, resource, dev）
   - `struct platform_driver` — 平台驱动（probe, remove, id_table）
   - `struct resource` — 硬件资源定义（IORESOURCE_MEM, IORESOURCE_IRQ, IORESOURCE_DMA）
   - `platform_device_register()` / `platform_driver_register()`

2. **匹配机制**
   ```
   1. driver_override 强制匹配
   2. OF (Device Tree) 风格匹配
   3. ACPI 风格匹配
   4. ID table 匹配 (platform_device_id)
   5. 名称直接匹配（fallback）
   ```

3. **驱动 probe 流程**
   - 获取资源：`platform_get_resource()`
   - 获取平台数据：`dev_get_platdata()`
   - 申请设备号 → cdev 初始化 → 添加字符设备 → 创建设备节点
   - `platform_set_drvdata()` — 将字符设备数据绑定到平台设备

4. **资源管理**
   - `devm_kzalloc()` — 设备生命周期绑定的内存分配（自动释放）
   - `devm_ioremap()` — 设备生命周期绑定的 IO 映射

5. **module_platform_driver** — 简化平台驱动注册宏

---

### my_driver/device/second_soft_timer/

**功能**：秒计时器字符设备，演示 **内核定时器** 和 **原子操作**

**源文件**：

| 文件 | 说明 |
|------|------|
| `second_cdev_timer.c` | 秒计时器字符设备驱动 |
| `test_demo.c` | 用户空间测试程序（每秒读取计时器值） |

**关键知识点**：

1. **内核定时器**
   - `struct timer_list` — 内核定时器结构体
   - `timer_setup()` — 初始化定时器并绑定回调函数
   - `add_timer()` — 将定时器添加到内核定时器队列
   - `mod_timer()` — 修改定时器超时时间
   - `del_timer_sync()` — 同步删除定时器（等待定时器处理完成）
   - `jiffies` — 内核全局时间滴答计数
   - `HZ` — 每秒滴答数（内核频率）

2. **原子操作**
   - `atomic_t` / `atomic_read()` / `atomic_inc()` / `atomic_dec_return()`
   - `atomic_inc_return()` / `atomic_cmpxchg()` — 原子比较交换

3. **自旋锁（Spinlock）**
   - `spinlock_t` / `spin_lock()` / `spin_unlock()` — 用于保护短临界区

4. **无锁初始化模式**
   - `atomic_cmpxchg()` + `smp_mb()` 确保单次初始化
   - 首次打开设备时启动定时器，关闭全部文件句柄时停止定时器

5. **数据传递**
   - `put_user()` — 从内核向用户空间传递单个整数

---

### my_driver/block_device/MMC_eMMC_virtual_disk/

**功能**：完全模拟 eMMC 存储设备，使用 Linux **MMC 子系统** 框架

**源文件**：

| 文件 | 说明 |
|------|------|
| `virtual_emmc.c` | 虚拟 eMMC 设备驱动（444行） |
| `test_demo.c` | 测试程序（占位） |

**关键知识点**：

1. **MMC 子系统**
   - `struct mmc_host` — MMC 主机控制器结构体
   - `mmc_alloc_host()` / `mmc_free_host()` — 分配/释放 MMC 主机
   - `mmc_add_host()` / `mmc_remove_host()` — 添加/移除 MMC 主机
   - `mmc_priv()` — 获取 MMC 控制器私有数据

2. **MMC 协议模拟**
   - `struct mmc_host_ops` — MMC 主机操作接口（request, set_ios, get_cd, get_ro）
   - 模拟关键命令：
     - CMD0: GO_IDLE_STATE
     - CMD1: SEND_OP_COND
     - CMD2: ALL_SEND_CID
     - CMD3: SET_RELATIVE_ADDR
     - CMD9: SEND_CSD
     - CMD17/18: 读单/多块
     - CMD24/25: 写单/多块

3. **eMMC 寄存器模拟**
   - OCR 寄存器（电压范围）
   - CID 寄存器（制造商 ID、产品名、序列号）
   - CSD 寄存器（设备特性、容量）
   - EXT_CSD 寄存器（扩展特性：HS200/HS400、eMMC 版本、扇区数）

4. **工作队列**
   - `INIT_WORK()` — 初始化工作队列
   - `schedule_work()` — 调度工作队列执行
   - `cancel_work_sync()` — 同步取消工作队列

5. **设备容量**
   - 512MB 虚拟存储空间（RAM 中分配）
   - 每扇区 512 字节
   - 支持最多 128 个 DMA 段

---

### my_driver/block_device/RAM_Backed_virtual_disk/

**功能**：完整的 **RAM Disk** 块设备驱动，使用**按需分配物理页**策略模拟磁盘

**源文件**：

| 文件 | 说明 |
|------|------|
| `ram_disk.c` | RAM 磁盘块设备驱动（688行） |
| `test_demo.c` | 测试程序（占位） |

**关键知识点**：

1. **块设备核心架构**
   - `struct gendisk` — 通用磁盘结构体
   - `blk_alloc_disk()` — 分配通用磁盘对象
   - `add_disk()` — 向系统注册磁盘
   - `set_capacity()` — 设置磁盘容量
   - `struct block_device_operations` — 块设备操作函数集
   - `RAMDISK_MAJOR` — 主设备号 1

2. **BIO 处理机制**
   - `submit_bio()` — 通用块层提交 bio 请求的处理函数
   - `struct bio` / `struct bio_vec` / `struct bvec_iter` — BIO 请求结构
   - `bio_for_each_segment()` — 遍历 BIO 中的所有段
   - `bio_endio()` / `bio_io_error()` — 完成/错误结束 BIO

3. **基数树（Radix Tree）管理**
   - `struct radix_tree_root` — 基数树根节点
   - `radix_tree_lookup()` — 按索引查找页面
   - `radix_tree_insert()` — 插入页面
   - `radix_tree_gang_lookup()` — 批量查找页面
   - `radix_tree_delete()` — 删除页面
   - `radix_tree_preload()` / `radix_tree_preload_end()` — 预加载内存保证原子插入

4. **按需内存分配**
   - 初始不占用物理内存
   - 写入数据时调用 `brd_insert_page()` 按需分配物理页
   - `alloc_page(GFP_NOIO | __GFP_ZERO | __GFP_HIGHMEM)` — 分配清零的高端内存页
   - `__free_page()` — 释放页面

5. **高端内存映射**
   - `kmap_atomic()` — 原子地临时映射物理页到内核地址空间
   - `kunmap_atomic()` — 解除原子映射

6. **队列设置**
   - `blk_queue_physical_block_size()` — 设置物理块大小（PAGE_SIZE）
   - `QUEUE_FLAG_NONROT` — 非旋转设备标志（SSD/RAM）
   - `QUEUE_FLAG_NOWAIT` — 无等待标志

7. **DebugFS 支持**
   - `debugfs_create_dir()` — 创建 debugfs 目录
   - `debugfs_create_u64()` — 创建 64 位调试文件
   - 用于监控每个 RAM 磁盘当前占用的页面数

8. **使用场景**
   - 安装驱动后生成 `/dev/ram0` ~ `/dev/ram15`（默认 16 个设备）
   - 可以格式化：`mkfs.ext4 /dev/ram0`
   - 可以挂载：`mount /dev/ram0 /mnt/ram0`
   - 可用于测试文件系统、性能基准测试

---

## 关键知识点总览

### 一、内核模块开发基础
| 知识点 | 涉及目录 |
|--------|----------|
| 模块入口/出口宏 | helloworld, 所有驱动 |
| 模块参数传递 | globalmem, globalfifo, led_char_device |
| 模块信息声明 | 所有驱动 |
| printk 内核打印 | 所有驱动 |

### 二、字符设备驱动
| 知识点 | 涉及目录 |
|--------|----------|
| 设备号管理 (alloc_chrdev_region / register_chrdev_region) | char_device, globalmem, globalfifo |
| cdev 生命周期 (init / add / del) | char_device, globalmem, globalfifo, led_char_device |
| file_operations 实现 | char_device, globalmem, globalfifo |
| 用户/内核空间数据拷贝 (copy_to/from_user) | char_device, globalmem, globalfifo |
| class/device 自动创建设备节点 | char_device, led_char_device |

### 三、设备驱动模型
| 知识点 | 涉及目录 |
|--------|----------|
| 总线模型 (bus_type) | device_module |
| 设备注册 (device_register) | device_module |
| 驱动注册 (driver_register) | device_module |
| 平台设备模型 (platform_device/driver) | led_platform_device |
| 硬件资源管理 (struct resource) | led_platform_device |
| 匹配机制 (match / id_table / of_match) | device_module, led_platform_device |

### 四、高级 IO 模型
| 知识点 | 涉及目录 |
|--------|----------|
| 阻塞 IO / 等待队列 | globalfifo |
| 非阻塞 IO / -EAGAIN | globalfifo |
| 轮询 (poll / select / epoll) | globalfifo |
| 异步通知 (fasync / SIGIO) | globalfifo |
| 互斥锁 (mutex) | globalmem, globalfifo |

### 五、硬件访问
| 知识点 | 涉及目录 |
|--------|----------|
| 内存映射 (ioremap / iounmap) | led_char_device, led_platform_device |
| IO 访问 (ioread32 / iowrite32) | led_char_device, led_platform_device |
| 物理地址直接操作 | led_char_device |
| 设备树 (Device Tree) | device_tree |
| OF 操作函数 | device_tree |

### 六、内核定时器与并发
| 知识点 | 涉及目录 |
|--------|----------|
| 内核定时器 (timer_list) | second_soft_timer |
| jiffies / HZ | second_soft_timer |
| 原子操作 (atomic_t) | second_soft_timer, globalfifo |
| 自旋锁 (spinlock) | second_soft_timer |
| 工作队列 (workqueue) | MMC_eMMC_virtual_disk |

### 七、块设备驱动
| 知识点 | 涉及目录 |
|--------|----------|
| gendisk 管理 | RAM_Backed_virtual_disk |
| BIO 处理流程 | RAM_Backed_virtual_disk |
| MMC 子系统 | MMC_eMMC_virtual_disk |
| 基数树 (Radix Tree) | RAM_Backed_virtual_disk |
| 高端内存映射 (kmap_atomic) | RAM_Backed_virtual_disk |
| 按需分配物理页 | RAM_Backed_virtual_disk |
| eMMC 协议模拟 | MMC_eMMC_virtual_disk |

---

## 编译与使用

### 环境要求
- Linux 内核源码树（`/lib/modules/$(uname -r)/build`）
- GCC 编译器
- ARM64 交叉编译工具链（如目标板为 ARM）

### 通用编译命令
```bash
cd my_driver/device/xxx/
make          # 编译内核模块和用户空间测试程序
insmod xxx.ko # 加载内核模块
./test_demo    # 运行测试程序
rmmod xxx      # 卸载内核模块
```

### 驱动加载顺序示例（device_module）
```bash
insmod xbus.ko    # 1. 注册总线
insmod xdev.ko    # 2. 注册设备
insmod xdrv.ko    # 3. 注册驱动（自动匹配）
./test_demo       # 4. 运行测试
rmmod xdrv        # 5. 卸载驱动
rmmod xdev        # 6. 卸载设备
rmmod xbus        # 7. 卸载总线
```

---

## Linux 设备驱动开发路线图

本项目按难度递进，建议学习顺序：

```
1. helloworld              → 内核模块入门
2. char_device             → 字符设备基础
3. globalmem               → 内存管理 + 互斥
4. globalfifo              → 阻塞/非阻塞 + 等待队列 + poll + 异步通知
5. led_char_device         → 硬件访问 + MMIO
6. device_module           → 总线/设备/驱动模型
7. device_tree             → 设备树操作
8. led_platform_device     → 平台设备模型
9. second_soft_timer       → 内核定时器
10. RAM_Backed_virtual_disk → 块设备驱动
11. MMC_eMMC_virtual_disk   → MMC子系统
```

---

## License

本项目代码采用 GPL 协议开源，适用于 Linux 内核模块开发学习与研究。
