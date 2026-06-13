
# memory_io简介

## 项目

## 介绍Linux系统的内核内存管理
### 内存
1. **内存管理**: 用户空间 + 内核空间
    - 内核空间: 3G-4G
    - 物理内存映射区
    - DMA + 常规内存映射区
    - 虚拟内存分配区
    - 高端内存映射区
    - 专用页面映射区
    - 系统保留映射区
2. **内存管理算法**
    - buddy算法, 把空闲的页面以2的n次方为单位进行管理
    - slab机制: 对buddy算法管理的内存进行二次管理 —— 分配大量小对象的后备缓存技术
    - 内存池机制
3. **内存存取**
    - 算法配置: mallopt()
    - 用户空间申请: malloc(), free(), brk(), mmap()
    - 内核空间申请: kmalloc(), kfree(), __get_free_pages(), vmalloc(), vfree()
        - vmalloc(): 申请内存时, 会进行内存的映射, 改变页表项
        - kmalloc(): 使用DMA和常规区域的页表项
    - slab机制: kmem_cache_create(), kmem_cache_alloc(), kmem_cache_free(), kmem_cache_destroy()
    - 内存池机制: mempool_create(), mempool_alloc(), mempool_free(), mempool_destroy()
### I/O
1. **I/O端口和I/O内存是两种东西**: 端口是独立的, 内存是挂载在内存空间的I/O
    - I/O可以在硬件上设计独立的I/O空间
    - I/O也可以直接挂接在内存空间
2. **I/O端口**
    - 依赖于硬件平台
    - 申请I/O端口: request_region(), release_region()
    - 访问流程: request_region -> inb/oub -> release_region
3. **I/O内存**
    - 控制器寄存器的内存
    - 先映射: ioremap, iounmap, devm_ioremap
    - 读写: readb, readw, readl, writeb, writew, writel
    - I/O内存申请: request_mem_region(), release_mem_region()
    - 访问流程: request_mem_region -> ioremap -> readb/writeb -> iounmap -> release_mem_region
4. **设备地址映射到用户空间**    
    - mmap(): 将用户空间的一段内存与设备内存关联, 访问用户空间地址, 实际上会转化为对设备的访问. mmap()必须以PAGE_SIZE为单位进行映射
    - mmap()代码逻辑
        - 在进程虚拟空间查找一块VMA
        - 将这块VMA进行映射
        - 如果设备驱动程序或者文件系统的file_operation定义了mmap()操作, 则调用它
        - 将这个VMA插入进程的VMA链表中
### MMU
1. **TLB(Translation Lookaside Buffer)**
    - 转换旁路缓存. 缓存少量的虚拟地址与物理地址的转换关系, 是转换表的Cache
2. **TTW(Translation Table walk)**
    - 转换表漫游. TLB中没有命中, 需要访问TTW中虚拟地址与物理地址的对应关系. 命中则写入到TLB中
3. **三级页表**
    - PGD
    - PMD
    - PTE
4. **四级页表**
    - PGD: Page Global Directory. 页全局目录. 顶级目录，每个进程独有; 指向 PUD 或直接指向大页（1GB 页面）
    - PUD: Page Upper Directory. 页上级目录. 指向 PMD 或 2MB 大页
    - PMD: Page Middle Directory. 页中间目录. 指向 PTE 或 2MB 大页（透明大页）
    - PTE: Page Table Entry. 页表项
## DMA
1. **DMA控制器**
    - 控制数据在外设和内存之间传输, 传输期间无需CPU介入. 传输完成通过中断通知CPU数据传输结束
2. **DMA和Cache一致性问题**: 确保二者访问的内存区域不会重叠
    - DMA控制外设与内存进行数据传输
    - Cache作为CPU对内存数据的缓存
    - 当DMA区域与Cache区域发生重叠时, Cache中的数据此时与内存中的数据可能并不一致, 此时会引发很多问题
3. **DMA编程**
    - 基本物质基础
        - 内存
        - DMA通道
        - 中断
        - 外设与内存的速度匹配问题
    - 接口
        - **申请DMA区域**
            - __get_free_pages(gfp_mask | GFP_DMA, order)
            - __get_dma_page()
            - static unsigned long dma_mem_alloc(int size);
        - **DMA地址掩码**: 设备并不一定能在所有的内存地址上执行DMA操作
            - int dma_set_mask(struct device *dev, u64 mask)
        - **一致性DAM缓冲区**: 分配一片DAM缓冲区, 为这片缓冲区产生设备可访问的地址
            - void *dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp);
            - void *dma_alloc_coherent(struct device *dev, size_t size, void *cpu_addr, dma_addr_t handle);
            - void *dma_alloc_writecombine(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp); - 分配一个写合并的DAM缓冲区
            - void pci_alloc_consistent(struct pci_dev *pdev, size_t size, dma_addr_t *dma_addrp);
            - void pci_free_consistent(struct pci_dev *pdev, size_t size, void *cpu_addr, dma_addr_t dma_addr);
        - **流式DMA映射**: 不是所有的DMA缓冲区都是驱动申请的, 很多情况下, 缓冲区来自较上层(如网卡驱动中的网络报文). 此时使用流式DMA
            - dma_addr_t dma_map_single(struct device *dev, void *buffer, size_t size, enum dma_data_direction direction);
            - void dma_unmap_single(struct device *dev, dma_addr_t dma_addr, size_t size, enum dma_data_direction direction);
            - void dma_sync_single_for_cpu(struct device *dev, dma_handle_t bus_addr, size_t size, enum dma_data_direction direction);
            - void dma_sync_single_for_device(struct device *dev, dma_handle_t bus_addr, size_t size, enum dma_data_direction direction);
        - **dmaengine标准API**
            - struct dma_chan *dma_request_slave_channel(struct device *dev, const char *name);
            - struct dma_chan *__dma_request_channel(const dma_cap_mask_t *mask, dma_filter_fn fn, void *fn_param);
            - void dma_release_channel(struct dma_chan *chan);
            - 编程范式
                - dmaengine_prep_slave_single(): 准备好DMA描述符
                - rx_desc->callback = xxx_dam_fini_callback()
                - rx_desc->callback_param = &xxx->rx_done
                - dmaengine_submit(rx_desc)
                - dma_async_issue_pending(xxx->rx_chan);
        - 一致性DAM映射和流式DMA映射对比
            - 一致性DMA映射
                - 长期存在, 驱动加载时申请, 卸载时释放
                - CPU和设备共享, 双方随时可访问
                - 硬件自动保证Cache一致性
            - 流式DMA映射
                - 一次性/短期, 每次传输前映射, 传输后立即解除
                - 某一时刻只属于一方(CPU或设备)
                - 驱动必须显式调用AP处理(dma_map_*, dma_unmap_*)
                - 适用于网络数据包, 文件系统数据块, 音视频流
    - CMA(Contiguous Memory Allocator)
        - 在嵌入式中, GPU, Camera, HDMI等都要预留大量连续内存
        - CMA机制可以做到不预留内存, 需要的时候才分配给这些设备
                    
        - 地址
            - 总线地址: 总线地址即物理地址. 当存在桥接电路时, 桥接电路会将I/O地址映射成不同的物理地址, 此时总线地址就对应了多个物理地址
            - 物理地址: 1
            - 虚拟地址: 



# 模块简介


## MMU


## 页表

1. **四级页表**
```
虚拟地址（48位有效）
┌──────┬──────┬──────┬──────┬────────────┐
│ PGD  │ PUD  │ PMD  │ PTE  │   Offset   │
│ 9位  │ 9位  │ 9位  │ 9位  │   12位     │
└──────┴──────┴──────┴──────┴────────────┘

物理内存
┌─────────────────────────────────────┐
│ PGD (Page Global Directory)         │  ← 一级目录
│  └─→ PUD (Page Upper Directory)     │  ← 二级目录
│       └─→ PMD (Page Middle Dir)     │  ← 三级目录
│            └─→ PTE (Page Table Entry)│  ← 四级目录
│                 └─→ 物理页面        │  ← 实际数据
└─────────────────────────────────────┘
```

## 内存




## 内存管理接口







