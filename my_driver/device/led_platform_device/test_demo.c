#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

/****************************************************
 * 
 * LED 测试程序
 * 
 * 用法:
 *   ./test_demo <0|1>        - 写入固定值 (0=关灯, 1=开灯)
 *   ./test_demo toggle       - 切换LED状态
 *   ./test_demo blink <n>    - 闪烁n次 (n=0表示无限闪烁)
 *
 * 设备节点: /dev/led0
 * 
 * 向设备写入非0值 -> 点亮灯
 * 向设备写入0     -> 熄灭灯
 * 
 ****************************************************/

#define LED_DEVICE_PATH "/dev/led0"

/**
 * @brief 打开LED设备文件
 * @return 文件描述符，失败返回-1
 */
static int open_led_device(void)
{
    int fd = open(LED_DEVICE_PATH, O_RDWR);
    if (fd < 0)
    {
        perror("Failed to open LED device");
        printf("Note: Make sure the LED driver module is loaded.\n");
        return -1;
    }
    printf("LED device opened: %s\n", LED_DEVICE_PATH);
    return fd;
}

/**
 * @brief 控制LED开关
 * @param fd 文件描述符
 * @param on 1=开灯, 0=关灯
 * @return 0成功, -1失败
 */
static int control_led(int fd, int on)
{
    char buf[16];
    int len;
    ssize_t ret;

    if (on)
    {
        printf("Turning LED ON...\n");
        len = snprintf(buf, sizeof(buf), "1");
    }
    else
    {
        printf("Turning LED OFF...\n");
        len = snprintf(buf, sizeof(buf), "0");
    }

    ret = write(fd, buf, len);
    if (ret < 0)
    {
        perror("Failed to write to LED device");
        return -1;
    }
    else if (ret != len)
    {
        fprintf(stderr, "Warning: wrote %zd bytes, expected %d\n", ret, len);
        return -1;
    }

    return 0;
}

int main()
{
    int fd = 0;
    fd = open_led_device();
    if (fd > 0)
    {
        control_led(fd, 1);
        control_led(fd, 0);
        close(fd);
    }
    return 0;
}