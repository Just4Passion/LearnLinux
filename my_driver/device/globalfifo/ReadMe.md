
# globalfifo简介

## 项目
```
帮助掌握阻塞和非阻塞IO的实现: 等待队列; 轮询函数
1. 数据存储空间属性: FIFO
2. FIFO中有数据时才能读出数据, 且读出后, 数据被取走
3. FIFO不是满的时候才能写入
4. 读FIFO进程可以唤醒因为FIFO满了被阻塞的写FIFO进程
5. 写FIFO进程可以唤醒因为FIFO为空被阻塞的读FIFO进程
```
1. **阻塞IO**
    - 访问时, 资源尚未满足, 阻塞这个进程, 让其休眠: 如何阻塞
    - 当资源满足时, 唤醒这个进程: 唤醒进程的地方一般发生在中断里, 可使用"等待队列"唤醒进程
2. **非阻塞IO**
    - 访问时, 资源尚未满足, 返回-EAGAIN
    - 进程可以选择访问访问, 或者不停地查询, 直到可以进行操作为止
3. **进程调度**
    - 进程状态切换
    - 进程调度
4. **异步通知**
    - 驱动主动通知应用程序进行访问
    - 一旦设备就绪，则主动通知应用程序：信号驱动的异步I/O
    - 信号是软件层次上对中断机制的一种模拟
5. **异步IO**
    - 

## 驱动编写逻辑: 阻塞IO和非阻塞IO
```
NON_BLOCK, add_wait_queue, __set_current_state, schedule, wake_up_interruptible, poll
```
1. **阻塞**
    - 检测到条件不满足, 则挂起进程, 触发进程调度
    - 当条件满足时唤醒进程
2. **非阻塞**
    - 条件不满足时, 立即返回
    - 通过轮询函数, 查询条件是否满足
3. **把进程加入到等待队列**
    - add_wait_queue
4. **把进程从等待队列中唤醒**
    - wake_up_interruptible

## 异步IO: 信号与通知
1. **信号**
    - 信号捕获: 
    - 信号忽略: 
    - 信号释放: kill_fasync(), 发送信号


## 设备异步通知机制：字符设备, 网络设备, 输入设备, 管道, FIFO
1. **步骤**
    - 支持F_SETOWN命令, 能在这个控制命令处理中设置filep->f_owner为对对应进程ID
    - 支持F_SETFL命令, 每当FASYNC标志改变时, 驱动程序中的fasync()函数将得以执行. 因此驱动中应该实现fasync()函数
    - 在设备资源可获得时, 调用kill_fasync()函数激发相应的信号
2. **相关函数**
    - fasync_helper(): 处理FASYNC标志变更的函数
        - 添加/删除异步通知项
    - kill_fasync(): 释放信号用的函数
        - 发送异步通知信号
3. **struct fasync_struct**
    - 用于管理异步I/O通知队列，当文件描述符上的状态发生变化时（如数据可读），内核可以通过这个结构找到需要通知的进程，并发送SIGIO 信号



## test_dome使用方法
    - 进入目录
    - 执行make编译
1. **安装驱动**
    - insmod globallfifo_char_device.ko
    - 查看安装结果: cat /proc/devices
        - 获取主设备号: 237 globalfifo
2. **创建设备节点**
    - mknod /dev/globalfifo c 237 0
3. **select_main**
    - 向驱动写入数据: echo "Hello Globalfifo Char Device" > /dev/globalfifo
    - 运行test_demo: 修改文件权限chmod 777 test_demo, 运行./test_demo
    - 输出结果:
        - Poll monitor: can be read
        - Poll monitor: can be written
4. **epoll_main**
    - 向驱动写入数据: echo "Hello Globalfifo Char Device" > /dev/globalfifo
    - 运行test_demo: 
    - 输出结果:
        - FIFO is not empty
5. **signal_main**
    - 运行test_demo: ./test_demo
    - 直接输入字符: 
6. **signalio_main**
    - 后台执行test_demo: ./test_demo &
    - 向驱动写入数据: echo "Hello Singal" > /dev/globalfifo
        - 输出结果: receive a signal from globalfifo, signalnum: 29
    - 从文件读取数据: cat /dev/globalfifo
        - 输出结果: hello
        - 输出结果: receive a signal from globalfifo, signalnum: 29

        
## 后台进程读取终端输入被停止: ./test_demo &, signal_main()
```
终端控制权问题
    当程序在后台运行时（./test_demo &），它不是前台进程组的成员
    终端（tty）只能被前台进程组读取
    后台进程试图从终端读取数据时，会触发 SIGTTIN 信号

```

1. **tcsetpgrp()**: 将进程设置为前台进程组. tcsetpgrp()是终端控制相关的函数








# 组成模块简介


## 任何调度和任务状态
```
/* Used in tsk->state: */
#define TASK_RUNNING			0x0000
#define TASK_INTERRUPTIBLE		0x0001
#define TASK_UNINTERRUPTIBLE		0x0002
#define __TASK_STOPPED			0x0004
#define __TASK_TRACED			0x0008
/* Used in tsk->exit_state: */
#define EXIT_DEAD			0x0010
#define EXIT_ZOMBIE			0x0020
#define EXIT_TRACE			(EXIT_ZOMBIE | EXIT_DEAD)
/* Used in tsk->state again: */
#define TASK_PARKED			0x0040
#define TASK_DEAD			0x0080
#define TASK_WAKEKILL			0x0100
#define TASK_WAKING			0x0200
#define TASK_NOLOAD			0x0400
#define TASK_NEW			0x0800
#define TASK_STATE_MAX			0x1000
```
1. **任务状态**
    - 运行状态
        - TASK_RUNNING: 运行状态
        - TASK_INTERRUPTIBLE: 可中断睡眠. 进程正在等待某个条件或信号, 可以被信号唤醒
        - TASK_UNINTERRUPTIBLE: 不可中断睡眠. 进程在等待硬件条件，不处理信号; 不能被信号唤醒，常用于磁盘I/O
        - __TASK_STOPPED: 暂停状态
        - __TASK_TRACED: 跟踪状态
    - 退出状态
        - EXIT_ZOMBIE: 僵尸状态. 进程已经结束但父进程未wait
        - EXIT_DEAD: 最终状态. 即将彻底释放
        - EXIT_TRACE: 被跟踪的退出状态
    - 特殊状态
        - TASK_DEAD: 进程即将被销毁
        - TASK_WAKEKILL: 可被致命信号唤醒的睡眠
        - TASK_WAKING: 正在被唤醒的过渡状态
        - TASK_NEW: 新创建的进程
        - TASK_PARKED：内核线程专用的睡眠状态
        - TASK_NOLOAD: 不计入负载统计


## 等待队列: 数据结构, 运行原理, 阻塞和唤醒





## Linux信号
1. **信号**
    - SIGHUP: 1, 挂起
    - SIGINT: 2，终端中断
    - SIGQUIT: 3, 终端退出
    - SIGILL: 4, 无效指令
    - SIGTRAP: 5, 跟踪陷阱
    - SIGIOT: 6, IOT陷阱
    - SIGBUS: 7, BUS错误
    - SIGFPE: 8, 浮点异常
    - SIGKILL: 9, 强行终止(不能被捕获或忽略)
    - SIGUSR1: 10, 用户自定义信号1
    - SIGSEGV: 11, 无效的内存段处理
    - SIGUSR2: 12, 用户定义的信号2
    - SIGPIPE: 13, 半关闭管道的写操作已经发生
    - SIGALRM: 14, 计时器到期
    - SIGTERM: 15, 终止
    - SIGSTKFLT: 16, 堆栈错误
    - SIGCHLD: 17, 子进程已经停止或退出
    - SIGCONT: 18, 如果停止了，继续执行
    - SIGSTOP: 19, 停止执行(不能被捕获或忽略)
    - SIGTSTP: 20, 终端停止信号
    - SIGTTIN：21, 后台进程需从终端读取输入
    - SIGTTOUT: 22, 后台进程需向终端写出
    - SIGURG: 23, 紧急的套接字事件
    - SIGXCPU: 24, 超额使用CPU分配的时间
    - SIGXFSZ: 25, 文件尺寸超额
    - SIGVTALRM: 26, 虚拟时钟信号
    - SIGPROF: 27, 时钟信号描述
    - SIGWINCH: 28, 窗口尺寸变化
    - SIGGIO: 29, I/O
    - SIGPWR: 30, 断电重启

2. **信号处理**
```
void (*signal(int signum, void (*handler)(int))(int);

typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
```




