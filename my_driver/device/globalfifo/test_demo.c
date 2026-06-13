
#include <fcntl.h>
#include <stdio.h>      // 用于 printf
#include <stdlib.h>     // 用于 exit
#include <string.h>     // 用于 bzero
#include <unistd.h>     // 用于 close

#include <sys/select.h> // 用于 FD_ZERO, FD_SET, FD_ISSET, select
#include <sys/ioctl.h>  // 用于 ioctl
#include <sys/epoll.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>


/***********************************************************************
 * 
 * select:
 *      特点
 *          每次调用都需要拷贝fd集合
 *          水平触发
 *      原理
 *          用户进程预先创建fd_set集合
 *          select将fd_set拷贝到内核空间
 *          内核线性扫描所有传入的fd， 检查是否有I/O事件就绪
 *          没有就绪, 内核会让进程睡眠，直到有事件发生或超时
 *          当有fd就绪或超时后，select返回，并修改fd_set来标识哪些fd就绪
 * 
 * epoll: 
 *      特点
 *          I/O 多路复用机制
 *          内核维护fd集合
 *          支持水平触发和边缘触发
 *      原理
 *          epoll_create创建epoll实例
 *          epoll_ctl注册需要监听的fd和事件
 *          epoll_wait等待事件发生，内核通过回调机制将有事件的fd加入到就绪队列
 *          epoll_wait返回时，仅返回就绪的fd列表
 *      接口
 *          epoll_create(): 创建epoll实例
 *          epoll_ctl(): 管理epoll监控列表
 *          epoll_wait(): 等待事件发生
 * 
 */


#define GLOBAL_FIFO_MAGIC 'f'
#define GLOBALFIFO_MEM_CLEAR _IO(GLOBAL_FIFO_MAGIC, 0)

#define BUFFER_LEN 20

#define MAX_LEN 100


int select_main(void)
{
    int fd = -1;
    int num = 0;

    char rd_ch[BUFFER_LEN];

    fd_set rfds, wfds;

    fd = open("/dev/globalfifo", O_RDONLY | O_NONBLOCK);
    if (fd != -1)
    {
        if (ioctl(fd, GLOBALFIFO_MEM_CLEAR, 0) < 0)
        {
            printf("ioctl command failed\r\n");
        }
        while (1)
        {
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);

            FD_SET(fd, &rfds);
            FD_SET(fd, &wfds);

            select(fd + 1, &rfds, &wfds, NULL, NULL);

            if (FD_ISSET(fd, &rfds))
            {
                printf("Poll monitor: can be read\r\n");
            }

            if (FD_ISSET(fd, &wfds))
            {
                printf("Poll monitor: can be written\r\n");
            }
        }
    }
    else
    {
        printf("Device open failure\r\n");
    }

    return 0;
}



int epoll_main(void)
{
    int ret = 0;
    int fd = -1;
    int epfd = -1;
    struct epoll_event ev_globalfifo;

    fd = open("/dev/globalfifo", O_RDONLY | O_NONBLOCK);
    if (fd != -1)
    {
        if (ioctl(fd, GLOBALFIFO_MEM_CLEAR, 0) < 0)
        {
            printf("ioctl command fialed\r\n");
        }

        /*创建一个epoll fd*/
        epfd = epoll_create(1);
        if (epfd < 0)
        {
            perror("epoll_create()");
            ret = -1;
            goto out;
        }

        bzero(&ev_globalfifo, sizeof(struct epoll_event));
        ev_globalfifo.events = EPOLLIN | EPOLLPRI;

        ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev_globalfifo);
        if (ret < 0)
        {
            printf("epoll_ctl()");
            ret = -1;
            goto out;
        }

        ret = epoll_wait(epfd, &ev_globalfifo, 1, 15000);
        if (ret < 0)
        {
            perror("epoll_wait()");
        }
        else if (ret == 0)
        {
            printf("No data input in FIFO within 15 seconds\r\n");
        }
        else
        {
            printf("FIFO is not empty\r\n");
        }

        ret = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &ev_globalfifo);
        if (ret < 0)
        {
            perror("epoll_ctl()");
            ret = -1;
            goto out;
        }
        ret = 0;
    }
    else 
    {
        printf("device open failure\r\n");
        ret = -1;
        goto out;
    }

out:
    if (epfd != -1)
    {
        close(epfd);
    }
    if (fd != -1)
    {
        close(epfd);
    }

    return ret;
}



/******************************************************************************************
 * 
 *                                      信号测试
 * 
 *******************************************************************************************/
void input_handler(int num)
{
    char data[MAX_LEN];
    int len;
    len = read(STDIN_FILENO, &data, MAX_LEN);
    data[len] = 0;

    printf("input available: %s\r\n", data);
}

/***********************************
 * 使用方法
 *      执行./test_demo: 不需要后台运行
 *      在终端输入字符
 * 
 */
void signal_main()
{
    int oflags;
    signal(SIGIO, input_handler);
    signal(SIGTERM, input_handler);

    fcntl(STDIN_FILENO, F_SETOWN, getpid());        //把文件所有者设置为测试进程, 这样通知的时候可以触发信号处理函数
    oflags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, oflags | FASYNC);  //启用异步通知机制

    while(1);
}


/***********************************
 * 使用方法
 *      执行./test_demo: 不需要后台运行
 *      在终端输入字符
 * 
 */
static void signalio_handler(int signum)
{
    printf("receive a signal from globalfifo, signalnum: %d\r\n", signum);
}

void signalio_main()
{
    int fd = -1;
    int oflags = 0;

    fd = open("/dev/globalfifo", O_RDWR, S_IRUSR | S_IWUSR);
    if (fd != -1)
    {
        signal(SIGIO, signalio_handler);
        
        fcntl(fd, F_SETOWN, getpid());

        oflags = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, oflags | FASYNC);
        while(1)
        {
            sleep(100);
        }
    }
    else
    {
        printf("device open failure\r\n");
    }
}


int main(void)
{
    //epoll_main();
    //select_main();
    //signal_main();
    signalio_main();
    return 0;
}


