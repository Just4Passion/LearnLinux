# led_char_device简介


## 项目
```
该项目是为了了解访问物理地址（寄存器地址，物理内存地址）的操作
    - ioremap, iounmap
    - iowrite, ioread
```

## GPIO




# 模块

## MMU
1. **物理地址**
    - 实际存在的，具备硬件实体的，CPU可以访问的存储空间的地址

2. **虚拟地址**
    - 虚拟的，软件属性的，通过共享硬件实现的，编译器支持的地址空间
    - 作用
        - 进程隔离
        - 内存保护
        - 内存扩展: 交换分区
        - 统一视图
3. **MMU**
    - CPU内部的硬件单元，负责将虚拟地址转换为物理地址
    - 使能MMU之后，外设寄存器的物理地址不能直接访问, 需要映射到虚拟地址空间

## 映射函数: ioremap/iounmap
1. **ioremap**
    - 工作原理  
        - 在内核的I/O映射区分配虚拟地址空间
        - 创建页表项，将虚拟地址映射到目标物理地址
        - 设置正确的内存属性
        - 返回映射后的虚拟地址
2. **ioremap变体**
    - ioremap_wc(): 写合并，用于显存等
    - ioremap_wt(): Write-Through, 写透
    - ioremap_cache(): 使用缓存
    - ioremap_np(): 非永久映射


## 访问函数: ioread/iowrite
1. **访问函数的作用**
    - 内存屏障: 确保I/O访问的顺序
    - 访问宽度: 确保使用正确的总线宽度
    - 字节序处理: 处理大小端问题
    - 防止编译器优化: volatile语义
2. **ioread**
    - u8  ioread8(void __iomem *addr);
    - u16 ioread16(void __iomem *addr);
    - u32 ioread32(void __iomem *addr);
    - u64 ioread64(void __iomem *addr);
3. **iowrite**
    - void iowrite8(u8 value, void __iomem *addr);
    - void iowrite16(u16 value, void __iomem *addr);
    - void iowrite32(u32 value, void __iomem *addr);
    - void iowrite64(u64 value, void __iomem *addr);

