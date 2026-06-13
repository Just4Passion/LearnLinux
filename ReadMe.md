# YeHuo_LuBanCat - Linux 设备驱动开发学习项目

## 📋 项目概述

本项目是一个基于 **Rockchip RK3528/RK3568 平台（野火鲁班猫）** 的 **Linux 设备驱动开发学习仓库**。项目从零开始系统地涵盖了 Linux 内核模块开发、字符设备驱动、并发与竞态控制、设备驱动模型（总线/设备/驱动）、设备树、中断与定时器、GPIO 与 pinctrl 子系统、块设备驱动、存储设备协议、I2C/SPI/USB 总线驱动、内存管理与 DMA、Regmap 寄存器映射等核心主题。

每个子目录都是一个独立的驱动实验项目，包含完整的源码（.c）、构建脚本（Makefile）、测试程序（test_demo）及详细的学习笔记（ReadMe.md），适合嵌入式 Linux 驱动开发者循序渐进地学习和参考。

---

## 📁 目录结构总览

```
YeHuo_LuBanCat/
├── ReadMe.md                              # 本文件 - 项目总文档
│
├── my_driver/                             # 主驱动学习目录
│   ├── ReadMe.md                          # 驱动学习总纲与知识点总结
│   ├── ReadMe_Driver.md                   # 驱动基本概念速览
│   ├── ReadMe_App.md                      # 应用层相关笔记
│   ├── ReadMe_Linux设备驱动架构.md         # Linux驱动架构深入分析
│   ├── ReadMe_RK3528.md                   # RK3528平台相关笔记
│   ├── ReadMe_编程规范.md                 # 代码编写规范
│   ├── Linux架构.drawio                   # Linux架构示意图
│   │
│   ├── helloworld/                        # [入门] 内核模块基础
│   ├── concurrency&race_conditions/       # [并发] 并发与竞态控制
│   │
│   ├── device/                            # [设备驱动] 核心实验目录
│   │   ├── char_device/                   # 字符设备驱动基础
│   │   ├── globalmem/                     # 字符设备 + 并发同步
│   │   ├── globalfifo/                    # 阻塞/非阻塞IO + 异步通知
│   │   ├── led_char_device/               # LED字符设备 + 物理地址映射
│   │   ├── led_dts_device/                # LED + 设备树访问
│   │   ├── led_platform_device/           # LED + 平台设备模型
│   │   ├── led_ai_code/                   # LED + AI优化综合示例
│   │   ├── device_module/                 # 设备驱动模型（总线/设备/驱动）
│   │   ├── device_tree/                   # 设备树解析
│   │   ├── dynamic_device_tree/           # 动态设备树（overlay）
│   │   ├── gpio_pinctrl/                  # GPIO + pinctrl子系统
│   │   ├── interrupt_button/              # 按键中断
│   │   ├── interrupt_timer/               # 中断 + 内核定时器
│   │   ├── soft_timer_led/                # 软定时器实现LED闪烁
│   │   ├── soft_timer_second/             # 内核定时器基础
│   │   ├── memory_io_DMA/                 # 内存管理 + DMA
│   │   ├── register_map/                  # Regmap寄存器映射
│   │   ├── async_io/                      # 异步IO
│   │   └── block_device/                  # 块设备驱动基础
│   │
│   ├── bus/                               # [总线] 总线型设备驱动
│   │   ├── i2c_MPU6050/                   # I2C总线 + MPU6050传感器
│   │   ├── spi_oled/                      # SPI总线 + OLED显示屏
│   │   ├── usb/                           # USB设备驱动
│   │   ├── rs232/                         # RS232串口（预留）
│   │   ├── rs485/                         # RS485总线（预留）
│   │   └── SDIO/                          # SDIO总线（预留）
│   │
│   ├── block_device/                      # [存储] 块设备与存储技术
│   │   ├── ReadMe.md                      # 块设备架构总览
│   │   ├── 存储器接口和协议概述.md         # 存储接口/协议对比
│   │   ├── 虚拟设备_内存盘.md             # 虚拟块设备分析
│   │   ├── NULL_Block_virtual_disk/       # NULL块设备（测试用）
│   │   ├── RAM_Backed_virtual_disk/       # 内存模拟磁盘
│   │   ├── RAM_Block_virtual_disk/        # RAM块设备
│   │   ├── MMC_eMMC_virtual_disk/         # MMC/eMMC子系统
│   │   ├── MMC_eMMC_rockchip/             # Rockchip平台MMC/eMMC
│   │   ├── PCIe_NVMe_virtual_disk/        # PCIe/NVMe虚拟磁盘（预留）
│   │   ├── SAS_SCSI_virtual_disk/         # SAS/SCSI虚拟磁盘（预留）
│   │   ├── SATA_AHCI_virtual_disk/        # SATA/AHCI虚拟磁盘（预留）
│   │   ├── SD_SD_virtual_disk/            # SD卡虚拟磁盘（预留）
│   │   └── MIPI_M-PHY_UFS_virtual_disk/   # MIPI M-PHY/UFS（预留）
│   │
│   ├── net_device/                        # [网络] 网络设备驱动（预留）
│   └── power_manage/                      # [电源] 电源管理（预留）
```

---

## 🔑 各目录功能详解

### 1. helloworld/ — 内核模块入门

**目标**：了解 Linux 内核模块的最小组成、编译与加载卸载流程。

**关键知识点**：
- 模块三要素头文件：`<linux/module.h>`、`<linux/init.h>`、`<linux/kernel.h>`
- `module_init()` / `module_exit()` — 模块入口/出口注册
- `MODULE_LICENSE` — 模块许可证声明（GPL 等）
- `module_param()` — 模块参数传递（支持 `charp`、`int` 等类型）
- `EXPORT_SYMBOL` — 符号导出，供其他模块使用
- Makefile 构建：`-C` 指定内核目录、`M=` 指定模块源码目录
- 模块操作命令：`insmod` / `rmmod` / `lsmod` / `modprobe` / `modinfo`
- 内核日志查看：`dmesg`
- `/sys/module/` — sysfs 中的模块信息接口
- `/lib/modules/$(uname -r)/modules.dep` — 模块依赖关系管理

---

### 2. concurrency&race_conditions/ — 并发与竞态

**目标**：系统掌握 Linux 内核中的并发控制机制及其适用场景。

**关键知识点**：

| 机制 | 原理 | 适用场景 | 能否用于中断 |
|------|------|---------|:----------:|
| **中断屏蔽** | 禁止本地CPU中断响应 | 单核简单保护 | ✅ |
| **原子操作** | 硬件CAS指令（LDREX/STREX） | 计数器、标志位 | ✅ |
| **自旋锁** | 忙等待，Test-And-Set | 短临界区（微秒级） | ✅ |
| **信号量** | 计数信号量，可睡眠 | 多线程资源池 | ❌ |
| **互斥体** | 二进制信号量+优先级继承 | 长临界区，避免优先级反转 | ❌ |

**深入理解**：
- 自旋锁在单核 CPU 中实际是禁用抢占
- 互斥体支持优先级继承，解决优先级反转问题
- 原子操作的 `atomic_cmpxchg()` 实现无锁互斥
- 读写锁、顺序锁、RCU 等高级机制

---

### 3. device/ — 设备驱动核心实验

这是项目最核心的目录，包含从基础到进阶的系列驱动实验。

#### 3.1 字符设备驱动基础

- **char_device/** — 字符设备开发全流程
  - `struct cdev`：VFS层的字符设备抽象
  - `struct file_operations`：文件操作函数集（open/read/write/ioctl/llseek）
  - 设备号管理：`alloc_chrdev_region()` / `register_chrdev_region()`
  - `class_create()` + `device_create()` 自动创建设备节点
  - 多子设备支持：单cdev管理 vs 多cdev管理

- **globalmem/** — 字符设备 + 并发控制
  - 内核内存分配：页分配器、SLAB分配器、vmalloc、DMA内存
  - `ioctl()` 命令定义与实现
  - 编译器屏障 `barrier()`、内存屏障 `mb()/rmb()/wmb()`
  - `ioread`/`iowrite` 与 `readl/writel` 的屏障语义

- **globalfifo/** — 阻塞/非阻塞IO + 异步通知
  - 等待队列 `wait_queue_head_t`：进程阻塞与唤醒机制
  - 阻塞IO：条件不满足时挂起进程
  - 非阻塞IO：条件不满足时返回 `-EAGAIN`
  - 轮询机制：`poll()` / `select()` / `epoll()`
  - 异步通知：`fasync_helper()` + `kill_fasync()` + `SIGIO`
  - 进程状态：`TASK_INTERRUPTIBLE` / `TASK_UNINTERRUPTIBLE` 等

#### 3.2 LED驱动系列（循序渐进）

- **led_char_device/** — 通过 ioremap 访问物理寄存器地址
  - MMU 概念：虚拟地址 ↔ 物理地址映射
  - `ioremap()` / `iounmap()` — I/O 内存映射
  - `ioread32()` / `iowrite32()` — 带屏障的寄存器访问

- **led_dts_device/** — 设备树 + LED 驱动
  - DTS/DTB/DTC 设备树工具链
  - OF（Open Firmware）函数：`of_find_node_by_name()`、`of_property_read_u32()`、`of_iomap()` 等
  - 平台总线匹配逻辑：driver_override → OF匹配 → ACPI → ID table → 名称匹配

- **led_platform_device/** — 平台设备模型
  - `platform_bus`：为 SOC 集成外设提供的虚拟总线
  - `struct platform_device`：设备资源载体
  - `struct platform_driver`：probe/remove 接口
  - `struct resource`：资源描述（MEM/IRQ/IO/DMA）
  - 设备树节点自动创建 platform_device

- **led_ai_code/** — 综合优化示例
  - 同时支持设备树和传统平台数据两种资源解析方式
  - `devm_*` 系列自动资源管理函数
  - 互斥锁保护寄存器操作（并发安全）
  - 完善的错误处理：`dev_err()`、`IS_ERR()`、`PTR_ERR()`

#### 3.3 设备驱动模型

- **device_module/** — 总线/设备/驱动分离
  - `struct bus_type`：总线管理，match 匹配方法
  - `struct device`：统一设备抽象
  - `struct device_driver`：统一驱动抽象
  - `struct attribute`：sysfs 属性文件，用户空间 ↔ 内核空间交互
  - 总线属性、设备属性、驱动属性的创建与导出

- **device_tree/** — 设备树详解
  - DTS 语法：节点名称、标签、属性（compatible/reg/status/ranges）
  - 特殊节点：aliases、chosen
  - 设备树绑定文档：`Documentation/devicetree/bindings/`
  - `/proc/device-tree` 运行时设备树查看
  - OF 函数大全：节点查找、属性提取、内存映射

- **dynamic_device_tree/** — 动态设备树
  - `/plugin/` 标记与 overlay 机制
  - fragment + target + `__overlay__` 格式
  - 新语法：`&label` 直接引用
  - uboot 合并设备树流程

#### 3.4 GPIO 与 pinctrl

- **gpio_pinctrl/** — 引脚管理与 GPIO 控制
  - **pinctrl 子系统**：引脚复用 + 电气特性配置（上下拉、驱动能力）
  - **GPIO 子系统**：通用输入输出 + 中断支持
  - 工作流程：设备驱动 → pinctrl 配置复用 → GPIO 子系统接管
  - `gpiod_get()` / `gpiod_direction_output()` / `gpiod_set_value()`
  - 多LED场景：`for_each_available_child_of_node()` 遍历子节点
  - `gpio_chip` 核心结构：direction_input/output、get/set、to_irq

#### 3.5 中断与定时器

- **interrupt_button/** — 按键中断
  - 中断申请：`request_irq()` / `devm_request_irq()`
  - 中断触发类型：上升沿/下降沿/高电平/低电平
  - 设备树中断描述：`interrupt-parent` + `interrupts` 属性

- **interrupt_timer/** — 中断 + 内核定时器
  - GIC（Generic Interrupt Controller）架构：SGI/PPI/SPI
  - Linux中断处理架构：
    - **顶半部（硬中断上下文）**：紧急硬件操作，不可睡眠
    - **底半部机制**：
      - 软中断（softirq）：极高频场景，不可睡眠
      - tasklet：基于软中断的抽象，同类型串行化
      - 工作队列（work_struct）：内核线程上下文，可睡眠
      - 线程化中断（threaded_irq）：专用内核线程
  - 中断亲和性：`irq_set_affinity()` 绑定中断到指定 CPU

- **soft_timer_led/** — 定时器实现 LED 闪烁
  - 内核定时器回调在软中断上下文执行（不能睡眠，必须使用自旋锁）
  - `timer_setup()` / `mod_timer()` / `del_timer_sync()`
  - 自旋锁在定时器回调中的应用

- **soft_timer_second/** — 内核定时器基础
  - `jiffies`：系统节拍计数器（从启动开始的时钟中断次数）
  - `HZ`：每秒时钟中断次数（编译期常量）
  - `msleep()` / `msleep_interruptible()`：睡眠延迟

#### 3.6 内存管理与 DMA

- **memory_io_DMA/** — 内存管理与 DMA
  - 内核空间内存布局：物理内存映射区、vmalloc区、高端内存映射区
  - 内存管理算法：buddy算法（页管理）、slab机制（小对象缓存）
  - 内核内存分配接口：`kmalloc()` / `vmalloc()` / `__get_free_pages()`
  - I/O 端口 vs I/O 内存
  - DMA 编程：
    - 一致性DMA映射：`dma_alloc_coherent()`（长期、CPU与设备共享）
    - 流式DMA映射：`dma_map_single()`（短期、一方独享）
    - DMA引擎标准API：`dma_request_slave_channel()` -> `dmaengine_prep_slave_single()` -> `dmaengine_submit()`
    - 一致性/流式DMA对比与选择
    - CMA（Contiguous Memory Allocator）：动态连续内存分配
  - 页表：四级页表（PGD → PUD → PMD → PTE）
  - MMU + TLB + TTW 工作原理

#### 3.7 Regmap 与寄存器管理

- **register_map/** — Regmap 统一寄存器映射框架
  - `struct regmap_config`：寄存器配置（地址/数据位宽、缓存策略）
  - `struct regmap`：寄存器实体，封装缓存、锁、底层通信
  - `struct regmap_bus`：抽象不同总线的读写实现
  - 优势：统一API、内置缓存、原子操作、批量操作
  - 案例：通过 regmap 操作 OLED 控制芯片 SSD1306

#### 3.8 块设备基础

- **device/block_device/** — 块设备驱动基础
  - 块设备核心数据结构：
    - `struct gendisk`：磁盘设备描述（分区、请求队列、操作函数）
    - `struct block_device`：块设备实例
    - `struct request_queue`：请求队列（I/O调度与排队）
    - `struct bio`：底层I/O请求块
  - 块设备操作函数：`struct block_device_operations`
  - I/O调度器：Noop、Anticipatory、Deadline、CFQ
  - 多队列机制（blk-mq）

---

### 4. bus/ — 总线型设备驱动

#### 4.1 I2C 总线 + MPU6050 传感器

**i2c_MPU6050/** — I2C 设备驱动完整示例

- **I2C 子系统架构**：
  - `i2c_adapter`：I2C适配器（由SoC厂商提供）
  - `i2c_algorithm`：通信方法（master_xfer）
  - `i2c_driver`：外设驱动（probe/remove）
  - `i2c_client`：I2C设备（通过设备树解析生成）
  - `i2c_msg`：传输消息结构

- **MPU6050 六轴运动传感器**：
  - 三轴MEMS陀螺仪（科里奥利效应）
  - 三轴MEMS加速度计（F=ma + 电容检测）
  - DMP（数字运动处理器）引擎
  - FIFO缓存 + 中断控制器
  - I2C时序：单字节/突发读写

#### 4.2 SPI 总线 + OLED 显示屏

**spi_oled/** — SPI 设备驱动示例

- **SPI 子系统架构**：
  - `spi_controller`：SPI主机控制器（transfer/set_cs/transfer_one）
  - `spi_driver`：SPI外设驱动（probe/remove）
  - `spi_device`：SPI设备节点
  - `spi_message` / `spi_transfer`：传输数据结构
  - 同步传输 `spi_sync()` vs 异步传输 `spi_async()`

- OLED驱动：SSD1306 控制芯片驱动

#### 4.3 USB 设备驱动

**usb/** — USB 设备接入流程（学习笔记）
  - USB设备分类
  - USB设备枚举与接入流程

#### 4.4 RS232 / RS485 / SDIO

- 预留目录，尚未填充代码

---

### 5. block_device/ — 块设备与存储技术深度研究

#### 5.1 存储介质分层

| 介质 | 特点 | 典型应用 |
|------|------|---------|
| **NOR Flash** | 独立地址线、XIP执行、小容量 | Bootloader、固件 |
| **NAND Flash** | 页读取、大容量、需ECC/坏块管理 | SSD、U盘、eMMC |

#### 5.2 存储设备接口与协议对比

| 设备 | 内部介质 | 控制器 | 外部接口 | Linux驱动框架 |
|------|---------|--------|---------|:-----------:|
| SPI NOR Flash | NOR | 简单SPI从机 | SPI | drivers/spi/ |
| SPI NAND Flash | NAND | 简单SPI从机 | SPI | drivers/spi/ |
| SD/TF 卡 | NAND | 智能控制器 | SD协议 | drivers/mmc/ |
| eMMC 芯片 | NAND | 智能控制器 | eMMC协议 | drivers/mmc/ |
| UFS 芯片 | NAND | 智能控制器 | UFS协议 | drivers/ufs/ |
| SATA SSD | NAND | 智能控制器 | AHCI协议 | drivers/scsi/ |
| NVMe SSD | NAND | 智能控制器 | NVMe协议 | drivers/nvme/ |

#### 5.3 关键存储协议详解

- **PCIe + NVMe**：高速串行差分信号、多Lane、点对点、多队列（65535队列）、峰值16GB/s
- **SATA + AHCI**：单命令队列（深度32）、NCQ支持、峰值550MB/s
- **SAS + SCSI**：企业级、交换式架构（数万设备）、双端口冗余
- **MIPI M-PHY + UFS**：移动设备、全双工、多队列、峰值4.6GB/s
- **MMC + eMMC**：嵌入式、半双工、无命令队列、BGA封装
- **SD + SD协议**：可插拔、便携、速度等级定义

#### 5.4 高级块设备框架

- **Device Mapper**：设备映射器（LVM2、dm-crypt、dm-multipath）
- **MD**：软件RAID实现
- **Loopback**：文件虚拟为块设备（ISO挂载）
- **NBD**：网络块设备
- **zram**：压缩内存块设备（交换空间）
- **RAM Disk**：内存模拟磁盘

#### 5.5 虚拟块设备实验

- **NULL_Block_virtual_disk/** — NULL块设备（null_blk）：最简块设备实现
- **RAM_Backed_virtual_disk/** — 内存后端虚拟磁盘
- **RAM_Block_virtual_disk/** — RAM块设备
- **MMC_eMMC_virtual_disk/** — MMC子系统虚拟eMMC
- **MMC_eMMC_rockchip/** — Rockchip平台MMC/eMMC适配

---

### 6. net_device/ 与 power_manage/

- **net_device/** — 网络设备驱动（目录预留）
- **power_manage/** — 电源管理（目录预留）

---

## 📚 关键知识点体系

### 一、Linux 驱动架构思想

**分层与分离**（核心设计哲学）：
- **驱动与设备分离**：通过总线匹配机制解耦
- **驱动分层**：核心层（通用逻辑）+ 设备底层（硬件相关）
- **主机与外设驱动分离**：SPI/I2C/USB 总线系统中，主机端只产生波形，外设端调用标准API
- **M×N 变为 M+N**：中间件隔离强关联，减少代码组合

### 二、设备驱动模型

```
                ┌──────────────┐
                │   用户空间    │
                │  应用程序    │
                └──────┬───────┘
                       │ 系统调用
                ┌──────▼───────┐
                │   VFS层      │
                │ file_operations│
                └──────┬───────┘
          ┌────────────┼────────────┐
          │            │            │
    ┌─────▼─────┐ ┌───▼────┐ ┌───▼────┐
    │ 字符设备   │ │块设备   │ │网络设备 │
    │   cdev    │ │gendisk │ │net_dev  │
    └───────────┘ └────────┘ └─────────┘
          │
    ┌─────▼──────────────────┐
    │  设备驱动模型核心       │
    │  bus_type + device     │
    │  + device_driver       │
    └────────────────────────┘
```

### 三、中断处理层级

| 层级 | 上下文 | 可睡眠 | 可调度 | 典型延迟 |
|------|:-----:|:-----:|:-----:|:-------:|
| 硬中断（顶半部） | 硬件中断 | ❌ | ❌ | 微秒级 |
| 软中断（softirq） | 软中断 | ❌ | ❌ | 微秒级 |
| tasklet | 软中断 | ❌ | ❌ | 微秒级 |
| 工作队列（work_struct） | 内核线程 | ✅ | ✅ | 毫秒级 |
| 线程化中断（threaded_irq） | 内核线程 | ✅ | ✅ | 毫秒级 |

### 四、并发控制选择指南

```
临界区长度极短（< 几微秒）？
├─ 是 → 在中断上下文？
│      ├─ 是 → 自旋锁 (spin_lock_irqsave)
│      └─ 否 → 自旋锁 / 原子操作
└─ 否 → 需要计数？
       ├─ 是 → 信号量 (sema_init)
       └─ 否 → 需要优先级继承？
              ├─ 是 → 互斥体 (mutex_lock)
              └─ 否 → 信号量 / 互斥体
```

### 五、设备树（DT）核心语法

```
/ {
    node-name@unit-address {
        compatible = "manufacturer,device";    // 驱动匹配关键
        reg = <address size>;                  // 寄存器地址/长度
        status = "okay" / "disabled";          // 设备状态
        #address-cells = <1>;                  // 子节点地址字段长度
        #size-cells = <1>;                     // 子节点大小字段长度
        
        child-node {
            // ...
        };
    };
};
```

### 六、模块/驱动开发基本流程

```
1. 编写模块代码
   ├── module_init() → 初始化函数
   │   ├── 申请设备号 (alloc_chrdev_region)
   │   ├── 初始化cdev (cdev_init + cdev_add)
   │   ├── 创建类 (class_create)
   │   └── 创建设备 (device_create)
   └── module_exit() → 清理函数

2. 编写Makefile
   obj-m := module.o
   make -C /kernel/dir M=$(pwd) modules

3. 加载/测试
   insmod module.ko → 查看 /proc/devices
   mknod /dev/xxx c major minor (如未自动创建)
   编写用户态测试程序

4. 卸载
   rmmod module_name
```

---

## 🔧 硬件平台

- **主控芯片**：Rockchip RK3528 / RK3568
- **开发板**：野火（EmbedFire）鲁班猫（LubanCat）
- **架构**：ARM64 (aarch64)
- **交叉编译器**：`aarch64-linux-gnu-`

---

## 🚀 学习路线建议

```
新手入门路线：
① helloworld/ → 模块基础
② device/char_device/ → 字符设备
③ device/globalmem/ → 并发控制
④ device/globalfifo/ → 阻塞/非阻塞IO
⑤ device/led_char_device/ → 物理地址映射
⑥ device/device_module/ → 设备驱动模型
⑦ device/device_tree/ → 设备树
⑧ device/led_dts_device/ → 设备树+LED
⑨ device/led_platform_device/ → 平台设备
⑩ device/gpio_pinctrl/ → GPIO子系统
⑪ device/interrupt_timer/ → 中断与定时器
⑫ device/register_map/ → Regmap

进阶路线：
⑬ bus/i2c_MPU6050/ → I2C总线驱动
⑭ bus/spi_oled/ → SPI总线驱动
⑮ device/block_device/ → 块设备基础
⑯ block_device/ → 存储协议与块设备深入
⑰ device/memory_io_DMA/ → DMA编程
```

---

## 📝 编程规范

项目遵循统一的代码组织规范（详见 `my_driver/ReadMe_编程规范.md`）：

```
/*****************************************************
 *                  模块简介
 ******************************************************/
/*****************************************************
 *                  宏定义
 ******************************************************/
/*****************************************************
 *                  类型定义
 ******************************************************/
/*****************************************************
 *                  函数声明
 ******************************************************/
/*****************************************************
 *                  全局变量
 ******************************************************/
/*****************************************************
 *                  辅助函数
 ******************************************************/
/*****************************************************
 *                  字符设备操作
 ******************************************************/
/*****************************************************
 *                  模块初始化和退出
 ******************************************************/
```

---

## 🔗 参考资源

- **Linux内核源码**：`Documentation/` 目录下的驱动开发文档
- **设备树绑定文档**：`Documentation/devicetree/bindings/`
- **Rockchip官方文档**：Rockchip 芯片手册与 TRM
- **MPU6050数据手册**：嘉立创商城搜索 MPU6050
- **野火鲁班猫社区**：EmbedFire 官方资料

---

*本项目为个人 Linux 驱动开发学习记录，所有代码和笔记均为学习过程中的积累与总结。*
