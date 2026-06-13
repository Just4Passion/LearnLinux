

# interrupt_clock简介


## 项目
```
Linux上的中断编程
Linux上的定时器编程
```


## 问题
1. **softirq, tasklet二者都是运行在软中断上下文, 有其一不就行了吗?**
    - 这是机制与策略的分离的设计
    - softirq: 机制
        - 非常有限
        - 没有"禁用", "启用", "调度"的细粒度控制, 只有全局的触发机制
        - 并发控制非常复杂(多核上的复杂性)
    - tasklet: 策略
        - 通过两个专用的软中断(TASKLET_SOFTIRQ 和 HI_SOFTIRQ), 实现了无限数量的动态"软中断"
        - 同类型的tasklet的串行化执行

2. **struct work_struct, threaded_irq二者都是运行在进程上下文, 有其一不就行了吗?**
    - threaded_irq
        - 线程绑定到中断, 由中断子系统管理
        - 是中断处理流程的一部分: 包含屏蔽中断, 执行顶半部, 唤醒中断线程, 中断线程执行, 重新启用中断
        - 可以控制中断流
    - struct work_struct
        - 线程属于工作池, 由工作队列子系统管理
        - 是中断处理流程的外部消费者, 通过shedule_work()异步调度线程执行
        - 无法控制中断流



# 模块介绍

## 中断
1. **简介**
    - 向量中断: 硬件提供中断服务程序入口, 不同中断号的中断有不同的入口地址
    - 非向量中断: 软件提供中断服务程序入口地址, 多个中断共享同一个入口地址, 再通过中断标识来识别具体是哪个z中断
2. **中断控制器**
    - 可编程中断控制器(PIC)
    - 通过读写PIC的寄存器, 可以屏蔽/使能某中断(MASK寄存器), 以及获取中断状态(PEND寄存器)
3. **ARM多核中断控制器**: GIC(Generic Interrupt Controller)
    - 支持3种类型的中断
        - SGI(Software Generated Interrupt): 软件产生的中断
            - 可用于多核的核间通信: 一个CPU可以通过写GIC的寄存器给另一个CPU产生中断
            - 多核调度的一些中断都是SGI产生的
                - IPI_WAKEUP
                - IPI_TIMER
                - IPI_RESCHEDULE
                - IPI_CALL_FUNC
                - IPI_CALL_FUNC_SINGLE
                - IPI_CPU_STOP
                - IPI_IRQ_WORK
                - IPI_COMPILETION
        - PPI(Private Peripheral Interrupt): 某个CPU私有外设的中断. 中断只能发给绑定的那个CPU
        - SPI(Shared Peripheral Interrupt): 共享外设中断. 中断可以路由到任何一个CPU
            - 中断irq与CPU绑定
                - extern int irq_set_affinity(unsigned int irq, const struct cpumask *m);
4. **中断ID**
    - ID0~ID15：这 16 个 ID 分配给 SGI。
    - ID16~ID31：这 16 个 ID 分配给 PPI。
    - ID32~ID1019：这 988 个 ID 分配给 SPI，像 GPIO 中断、串口中断等这些外部中断
5. **设备树描述中断**: interupts属性
```
key {
    compatible = "alientek,key";
    pinctrl-names = "alientek,key";
    pinctrl-0 = <&key_gpio>;
    key-gpio = <&gpio3 RK_PC5 GPIO_ACTIVE_HIGH>;
    interrupt-parent = <&gpio3>;
    interrupts = <21 IRQ_TYPE_EDGE_BOTH>;
    status = "okay";
};
```





## Linux中断处理程序架构: 上半部(紧急的硬件操作), 下半部(延缓的耗时操作)
    - 许多操作系统都提供了中断上下文和非中断上下文相结合的机制, 将中断的耗时工作保留到非中断上下文处理
        - 比如把耗时工作挂到工作队列中



## Linux中断编程

1. **申请和释放中断**: 使用中断的设备需要申请和释放对应的中断
    - int request_irq(unsinged int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev);
        - flags: 中断处理的属性. 可以指定中断的触发方式
            - IRQF_TRIGGER_RISING
            - IRQF_TRIGGER_FALLING
            - IRQF_TRIGGER_HIGH
            - IRQF_TRIGGER_LOW
        - dev: 传递给中断服务的私有数据
    - int devm_request_irq(struct device *dev, unsinged int irq, irq_handler_t handler, unsigned long flags, const char *name, void *dev_id);
    - void free_irq(unsigned int irq, void *dev_id): 释放
2. **使能和屏蔽中断**
    - void disable_irq(int irq)： 等待目前的中断处理完成
    - void disable_irq_nosync(int irq): 理解返回
    - void enable_irq(int irq);
    - void local_irq_disable(), local_irq_save(flags): 屏蔽本CPU内的所有中断
    - void local_irq_enable(), local_irq_restore(flags): 恢复CPU内的所有中断

## Linux中断底半部机制: tasklet, 工作队列, 软中断, 线程化irq
```
┌─────────────────────────────────────────────────┐
│                    用户空间                       │
│              (Ring 3, 可调度, 可睡眠)             │
├─────────────────────────────────────────────────┤
│                   内核进程上下文                   │
│   工作队列、线程化中断 (可调度, 可睡眠, 有task_struct)│
├─────────────────────────────────────────────────┤
│                   软中断上下文                     │
│     softirq, tasklet (不可调度, 不可睡眠, 无独立栈) │
├─────────────────────────────────────────────────┤
│                   硬中断上下文                     │
│       ISR顶半部 (不可调度, 不可睡眠, 关中断)        │
└─────────────────────────────────────────────────┘
```

### 软中断: 一种底半部处理机制. 注册软中断, 触发软中断. 不允许睡眠
1. **底层原理**
    - 寄生在中断返回路径上
    - 其本质是运行在中断的上下文环境(可以结合MCU理解), 没有独立的栈, 和所有中断一样, 共享全局中断环境的栈
    - 既然是在中断的上下文环境运行, 那么就是不可调度(中断触发即处理), 不可睡眠的
    - 执行路径: irq_exit() → __do_softirq() → softirq_vec[]
2. **编程范式**
    - softirq_action: 表征一个软中断
    - open_softirq(): 注册一个软中断对应的处理函数
    - raise_softirq(): 触发一个软中断
    - local_bh_disable(): 禁止和使能软中断及tasklet底半部
    - local_bh_enable(): 禁止和使能软中断及tasklet底半部
3. **使用场景**
    - 特点
        - softirq驱动者是中断返回路径
        - 它没有自己的线程，没有自己的栈，它是寄生在中断返回机制上的
            - 中断返回机制: 执行硬件中断 -> 检查是否有软中断挂起 -> 执行软中断
    - 适用场景
        - 极高频的中断处理(如每秒数万次网络包)
        - 需要跨CPU并行处理
        - 对延迟极度敏感
        - 内核核心子系统(网络、块设备、定时器)

### tasklet**: 执行上下文是软中断. 执行时机是顶半部返回的时候. 不允许睡眠
1. **底层原理**
    - 是建立在软中断上的抽象
    - 本质是软中断
    - 它是通过两个特定的软中断来实现的: TASKLET_SOFTIRQ / HI_SOFTIRQ 软中断
    - 它是运行在中断返回路径上的, 所以说它是运行在软中断上下文的
    - 其本质是运行在中断的上下文环境, 没有独立的栈, 共享中断环境的栈
    - 执行路径: irq_exit() → __do_softirq() → softirq_vec[]
2. **编程范式**
```
void xxx_do_tasklet(unsigned long);                 //这是一个底半部处理函数
DECLARE_TASKLET(xxx_tasklet, xxx_do_tasklet, 0);    //声明一个tasklet, 和底半部处理函数绑定

/*中断处理底半部*/
void xxx_do_tasklet(unsigned long param)
{
    ...
}

/*定义一个顶半部函数, 执行上下文是硬中断**
irqreturn_t xxx_interrupt(int irq, void *dev_id)
{
    ...
    tasklet_schedule(&xxx_tasklet);     //顶半部结束的时候, 调度低半部的tasklet运行
    ...
}

/*模块初始化*/
int __init xxx_init(void)
{
    ret = request_irq(xxx_irq, xxx_interrupt, 0, "xxx", NULL);  //申请中断
}

/*模块退出*/
void __exit xxx_exit(void)
{
    free_irq(xxx_irq, xxx_interrupt);   //释放中断
}

```
3. **使用场景**
    - 特点
        - 本质上就是软中断
        - 它是通过两个特定的软中断来实现的: TASKLET_SOFTIRQ / HI_SOFTIRQ 软中断
        - 建立在 Softirq 之上的抽象
        - 不可睡眠, 不可调度, 没有独立栈
    - 适用场景
        - 大多数普通设备驱动(网卡, 串口, I2C, SPI等)
        - 需要简单易用的API
        - 同一设备的中断处理需要串行化

### 工作队列: struct work_struct, 执行上下文是内核线程, 可以调度和睡眠
1. **底层原理**
    - 本质是内核线程
    - 有自己独立的栈, 可以被调度(Linux调度器进行调度), 可以被睡眠
    - 执行路径: schedule_work() → insert_work() → wake_up_worker()
2. **编程范式**
```
struct work_struct xxx_wq;      //定义工作队列
void xxx_do_work(struct work_struct *work); //定义关联函数

/*中断处理底半部*/
void xxx_do_work(struct work_struct *work)
{

}

/*中断处理顶半部*/
irqreturn_t xxx_interrupt(int irq, void *dev_id)
{
    ...
    schedule_work(&xxx_wq);
    ...
}

int __init xxx_init(void)
{
    ret = request_irq(xxx_irq, xxx_interrupt, 0, "xxx", NULL);  //申请中断
    ...
    INIT_WORK(&xxx_wq, xxx_do_work);    //初始化工作队列
}

void __exit xxx_exit(void)
{
    free_irq(xxx_irq, xxx_interrupt);   //释放中断
}

```
3. **使用场景**
    - 特点
        - 本质是内核线程
    - 适用场景
        - 需要睡眠的操作（I2C/SPI 通信、内存分配等）
        - 需要互斥锁保护的临界区
        - 耗时较长的操作（毫秒级或更长）
        - 对延迟不敏感的场景
        - 需要访问用户空间或文件系统


### 线程化中断: thread_irq. 进程上下文
1. **底层原理**
    - 为每个中断创建专用内核线程
    - 执行路径: irq_wake_thread() → wake_up_process() → irq_thread()
2. **编程范式**
    - request_threaded_irq: 申请一个中断号, 并为这个中断号分配一个内核线程
    - devm_request_threaded_irq:

3. **使用场景**
    - 特点
        - 同一个中断的线程是串行执行的
    - 适用场景
        - 需要睡眠的操作（I2C、SPI、GPIO 扩展器等慢速总线）
        - 中断处理逻辑复杂（状态机、错误恢复等）
        - 需要实时优先级控制
        - 希望简化中断处理流程（减少 tasklet/工作队列的嵌套）
        - 电平触发中断的防抖处理

## 延时
1. **忙等延时**
    - void ndelay(unsigned long nsecs);
    - void udelay(unsigned long usecs);
    - void mdelay(unsigned long msecs);
    - time_after()
    - time_before()
2. **睡眠延迟**
    - unsigned long msleep_interruptible(unsigned int msecs);
    - void msleep(unsigned int msecs);  
        - 可调度
        - 本质是向系统添加一个定时器, 在定时器处理函数中唤醒与参数对应的进程

## delayed_work: struct work_struct和struct timer_list的结合. 工作队列+定时器实现周期性任务





