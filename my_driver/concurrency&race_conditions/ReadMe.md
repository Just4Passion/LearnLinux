
# concurrency&race_conditions

## 项目
```
介绍并发中解决竞态的方法
0. 中断屏蔽
1. 原子操作
2. 自旋锁
3. 信号量
4. 互斥体
```

# 模块介绍

## 原子操作
1. **底层原理**: 硬件依赖. 这是物质基础决定上层建筑
    - 保证对单个变量的操作不可被中断, 通过硬件级别的原子指令实现
2. **接口**
    - int atomic_cmpxchg(atomic_t *v, int old, int new)
        - 使用原子比较交换（CAS）实现无锁互斥
        - 读取 *ptr 的当前值, 如果当前值等于 old，则将 *ptr 设置为 new, 返回旧值
    - void atomic_set(atomic_t *v, int i);
    - void atomic_read(const atomic_t *v);
3. **编程范式**
```
/* 定义原子变量作为锁 */
static atomic_t device_lock = ATOMIC_INIT(0);

/* 获取锁 - 非阻塞版本 */
int try_acquire_lock(void)
{
    /* 如果锁空闲(0)，则占用(1)，否则返回失败 */
    if (atomic_cmpxchg(&device_lock, 0, 1) != 0) {
        return -EBUSY;  /* 锁已被占用 */
    }
    return 0;  /* 成功获取锁 */
}

/* 释放锁 */
void release_lock(void)
{
    atomic_set(&device_lock, 0);
}
```
4. **适用场景**
    - 特点
        - 无睡眠，性能最高
    - 适用场景
        - 简单的计数器操作（如引用计数）
        - 标志位的设置和清除
        - 单个变量的简单算术运算
        - 中断上下文可用
        - 需要非阻塞操作



## 自旋锁: 适用于SMP
1. **底层原理**
    - 当锁被占用时，线程会"自旋"等待（忙等待），不放弃CPU
2. **接口**
    - 初始化
        - void spin_lock_init(spinlock_t *lock);
        - void rwlock_init(rwlock_t *lock);
    - 加锁/解锁
        - void spin_lock(spinlock_t *lock);
        - void spin_unlock(spinlock_t *lock);
        - int spin_trylock(spinlock_t *lock);
        - int spin_is_locked(spinlock_t *lock);
    - 中断加锁/解锁
        - void spin_lock_irq(spinlock_t *lock);
        - void spin_unlock_irq(spinlock_t *lock);
        - void spin_lock_irqsave(spinlock_t *lock, unsigned long flags);
        - void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);
        - void spin_lock_bh(spinlock_t *lock);
        - void spin_unlock_bh(spinlock_t *lock);
3. **编程范式**
``
#include <linux/spinlock.h>

// 定义自旋锁
spinlock_t my_lock = SPIN_LOCK_UNLOCKED;

// 在进程上下文使用
spin_lock(&my_lock);
// 临界区代码
spin_unlock(&my_lock);

// 在中断上下文使用（保存中断状态）
unsigned long flags;
spin_lock_irqsave(&my_lock, flags);
// 临界区代码
spin_unlock_irqrestore(&my_lock, flags);
``
4. **适用场景**
    - 特点
        - 
    - 适用场景
        - 锁持有时间非常短（微秒级别）
        - 中断上下文必须使用自旋锁
        - 多核处理器上的短期保护



## 信号量: 不可用于中断
1. **底层原理**
    - 计数信号量，允许有限数量的线程同时访问资源
2. **接口**
    - 初始化
        - void sema_init(struct semaphore *sem, int val);
        - void init_rwsem(struct rw_semaphore *sem);
    - 获取信号量
        - void down(struct semaphore *sem);                   // 不可中断的阻塞等待
        - int down_interruptible(struct semaphore *sem);      // 可被信号中断
        - int down_killable(struct semaphore *sem);           // 可被致命信号中断
        - int down_trylock(struct semaphore *sem);            // 非阻塞尝试
        - int down_timeout(struct semaphore *sem, long jiffies); // 超时等待
    - 释放信号量
        - void up(struct semaphore *sem);
        - void up_read(struct rw_semaphore *sem);
        - void up_write(struct rw_semaphore *sem);

3. **编程范式**
```
#include <linux/semaphore.h>

// 定义信号量
struct semaphore my_sem;

// 初始化（允许3个线程同时访问）
sema_init(&my_sem, 3);

// 获取信号量（可能睡眠）
if (down_interruptible(&my_sem)) {
    // 被信号中断
    return -ERESTARTSYS;
}

// 临界区代码

// 释放信号量
up(&my_sem);
```
4. **适用场景**
    - 特点
        - 可睡眠，支持计数
    - 适用场景
        - 需要限制同时访问资源的线程数量
        - 允许多个读者同时访问（读写信号量）
        - 资源数量有限的情况（如DMA缓冲区池）



## 互斥体: 不可用于中断
1. **底层原理**
    - 二进制信号量，同一时刻只允许一个线程访问资源
    - 支持优先级继承，避免优先级反转
2. **接口**
    - 初始化
        - void mutex_init(struct mutex *lock);
    - 加锁
        - void mutex_lock(struct mutex *lock);
        - int mutex_lock_interruptible(struct mutex *lock);  // 返回 -EINTR 如果被信号中断
        - int mutex_lock_killable(struct mutex *lock);       // 返回 -EINTR 如果被致命信号中断
        - int mutex_lock_io(struct mutex *lock);             // 等待时可被IO信号中断
        - int mutex_trylock(struct mutex *lock);  // 成功返回1，失败返回0
    - 解锁
        - void mutex_unlock(struct mutex *lock);
    - 状态检查
        - int mutex_is_locked(struct mutex *lock);  // 已锁返回1

3. **编程范式**
```
#include <linux/mutex.h>

// 定义互斥体
struct mutex my_mutex;

// 初始化
mutex_init(&my_mutex);

// 获取互斥体（可能睡眠）
mutex_lock(&my_mutex);

// 临界区代码

// 释放互斥体
mutex_unlock(&my_mutex);

// 非阻塞尝试
if (mutex_trylock(&my_mutex)) {
    // 成功获取锁
    mutex_unlock(&my_mutex);
} else {
    // 锁被占用
}
```
4. **适用场景**
    - 特点
        - 可睡眠，支持优先级继承
    - 适用场景
        - 锁持有时间较长（毫秒级别以上）
        - 进程上下文中的互斥访问
        - 需要避免优先级反转的实时系统


