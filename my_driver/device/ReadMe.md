
# 目录简介
```
device
    ....设备模型
        device_module

    ....设备树
        device_tree
        dynamic_device_tree    

    ····字符设备
        char_device: 字符设备, 基本开发流程
        globalfifo: 字符设备, 阻塞IO和非阻塞IO, 异步IO
        globalmem: 字符设备, 并发和同步. 互斥
        led_char_device: 字符设备. 物理地址到虚拟地址的映射: ioremap, iounmap, ioread, iowrite
        led_dts_device: 字符设备. 访问设备树
        led_platform_device: 字符设备. 无设备树, 定义平台设备
        led_ai_code: 字符设备. 综合设备树+struct resource

    ...GPIO驱动
        gpio_pinctrl: gpio资源, gpio配置. 复用, 方向, 驱动能力; 电平输出输出

    ...中断和时钟
        interrupt_timer: 按键中断, 定时器
        interrupt_button: 按键中断
        soft_timer_led: 定时器. 定时器回调; 自旋锁. 使用内核定时器实现LED闪烁
        soft_timer_second: 定时器
    
    ...内存映射和DMA
        memory_io_DMA
        register_map: 将寄存器地址直接映射到内核空间, 直接访问寄存器内容


    ....块设备



    ....网络设备


    
```



# 基本概念
    - 模块: 参数, 符号, 装载, 卸载, 许可证
    - 字符设备
    - 设备, 驱动, 总线, 类
    - 属性
    - 资源
    - 设备树
    - 阻塞IO与非阻塞IO: 等待队列
    - 异步IO: 信号, 通知, AIO
    - 平台设备
    - 设备树: of_node节点, of函数
    - 定时器
    - 中断
        - 硬中断, 软中断, tasklet, struct work_struct, threaded_irq
    - 内存管理
        - 内存映射
        - I/O端口映射
        - DMA
        - Cache
    - 寄存器管理: Regmap

1. **资源存放的位置**
    - 可以直接放在驱动中
    - 存放在struct resource中
    - 存放在设备树中, 通过of函数引用
    - 实际根据平台选择, 如果支持设备树, 就使用dev->of_node; 不支持, 就是用struct resource
2. **错误处理**
    - dev_err: 设备错误日志. 时间戳 + 驱动名 + 设备名 + 错误消息
    - IS_ERR(): 判断是否错误指针
    - PTR_ERR(): 从错误指针提取错误码
3. **动态设备树**
    - 格式
    - 加载: 通过uboot加载
        - 设备树编译后生成: .dbt, .dtbo, 放在boot分区
        - uboot合并设备树, 传递设备树地址给kernel
4. **优先级反转**
    - 低优先级持有高优先级的锁
    - 解决方法
        - 优先级继承：当高优先级任务等待低优先级任务持有的锁时，临时提升低优先级任务的优先级
        - 优先级天花板: 获取锁时，立即将任务优先级提升到可能访问该锁的所有任务中的最高优先级
        - 禁止抢占（禁用中断）: 在持有锁期间禁止抢占，防止中优先级任务打断

# 问题

1. **在编译内核时, 如何添加和删除不同的模块, 以减小内核的大小, 或支持某个功能**
2. **当存在总线, 驱动, 设备的模块时, 如何编写它们之间的加载关系; 如何定义依赖模块之间的加载关系**




