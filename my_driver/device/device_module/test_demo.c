/**
 * test_demo.c - device_module 用户空间测试程序
 * 
 * 功能说明：
 *   通过 sysfs 文件系统与内核模块进行交互，测试总线、设备、驱动的属性读写。
 * 
 * 内核模块结构：
 *   - xbus.c:  注册 "xbus" 总线，提供总线属性 xbus_test
 *   - xdev.c:  注册 "xdev" 设备，提供设备属性 xdev_id (可读可写)
 *   - xdrv.c:  注册 "xdrv" 驱动，提供驱动属性 drvname (只读)
 * 
 * 匹配机制：
 *   xbus_match() 通过 strncmp 比较 device->name 和 driver->name
 *   xdev.name = "xdev", xdrv.name = "xdev" -> 匹配成功
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* sysfs 路径定义 */
#define SYSFS_BUS_PATH      "/sys/bus/xbus"
#define SYSFS_BUS_ATTR      "/sys/bus/xbus/xbus_test"
#define SYSFS_DEVICE_PATH   "/sys/devices/xdev"
#define SYSFS_DEV_ATTR      "/sys/devices/xdev/xdev_id"
#define SYSFS_DRIVER_PATH   "/sys/bus/xbus/drivers/xdev"
#define SYSFS_DRV_ATTR      "/sys/bus/xbus/drivers/xdev/drvname"

/* 颜色定义，用于终端输出美化 */
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_CYAN    "\033[36m"

/* 打印分隔线 */
void print_separator(const char *title)
{
    printf("\n" COLOR_CYAN "========================================" COLOR_RESET "\n");
    if (title)
        printf(COLOR_CYAN "  %s" COLOR_RESET "\n", title);
    printf(COLOR_CYAN "========================================" COLOR_RESET "\n\n");
}

/* 读取 sysfs 属性文件内容 */
int read_sysfs_attr(const char *path, char *buf, size_t size)
{
    int fd;
    ssize_t count;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf(COLOR_RED "[ERROR] 打开文件失败: %s" COLOR_RESET "\n", path);
        printf(COLOR_RED "[ERROR] 原因: %s" COLOR_RESET "\n", strerror(errno));
        return -1;
    }

    count = read(fd, buf, size - 1);
    if (count < 0) {
        printf(COLOR_RED "[ERROR] 读取失败: %s" COLOR_RESET "\n", strerror(errno));
        close(fd);
        return -1;
    }
    buf[count] = '\0';

    close(fd);
    return 0;
}

/* 写入 sysfs 属性文件内容 */
int write_sysfs_attr(const char *path, const char *buf)
{
    int fd;
    ssize_t count;
    size_t len = strlen(buf);

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        printf(COLOR_RED "[ERROR] 打开文件失败: %s" COLOR_RESET "\n", path);
        printf(COLOR_RED "[ERROR] 原因: %s" COLOR_RESET "\n", strerror(errno));
        return -1;
    }

    count = write(fd, buf, len);
    if (count < 0) {
        printf(COLOR_RED "[ERROR] 写入失败: %s" COLOR_RESET "\n", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

/* 检查文件是否存在 */
int file_exists(const char *path)
{
    access(path, F_OK);
    return (errno == 0);
}

/* ==================== 测试1: 总线属性 ==================== */
void test_bus_attribute(void)
{
    char buf[64];

    print_separator("测试1: 总线属性 (xbus_test)");

    if (!file_exists(SYSFS_BUS_ATTR)) {
        printf(COLOR_YELLOW "[WARN] 总线属性文件不存在: %s" COLOR_RESET "\n", SYSFS_BUS_ATTR);
        printf(COLOR_YELLOW "[INFO] 请先加载内核模块: insmod xbus.ko" COLOR_RESET "\n\n");
        return;
    }

    printf(COLOR_BLUE "[INFO] 读取总线属性: %s" COLOR_RESET "\n", SYSFS_BUS_ATTR);
    if (read_sysfs_attr(SYSFS_BUS_ATTR, buf, sizeof(buf)) == 0) {
        printf(COLOR_GREEN "[RESULT] 总线名称: %s" COLOR_RESET "\n", buf);
    }

    printf("\n");
}

/* ==================== 测试2: 设备属性 ==================== */
void test_device_attribute(void)
{
    char buf[64];
    char write_buf[32];
    int new_id;

    print_separator("测试2: 设备属性 (xdev_id)");

    if (!file_exists(SYSFS_DEV_ATTR)) {
        printf(COLOR_YELLOW "[WARN] 设备属性文件不存在: %s" COLOR_RESET "\n", SYSFS_DEV_ATTR);
        printf(COLOR_YELLOW "[INFO] 请先加载内核模块: insmod xdev.ko" COLOR_RESET "\n\n");
        return;
    }

    /* 2.1 读取初始值 */
    printf(COLOR_BLUE "[INFO] 步骤1: 读取 xdev_id 初始值" COLOR_RESET "\n");
    if (read_sysfs_attr(SYSFS_DEV_ATTR, buf, sizeof(buf)) == 0) {
        printf(COLOR_GREEN "[RESULT] 当前 xdev_id = %s" COLOR_RESET "\n", buf);
    }

    /* 2.2 写入新值 */
    printf("\n");
    printf(COLOR_BLUE "[INFO] 步骤2: 写入新的 xdev_id 值" COLOR_RESET "\n");
    printf(COLOR_BLUE "[INFO] 请输入新值 (0-999): " COLOR_RESET);
    if (scanf("%d", &new_id) != 1 || new_id < 0 || new_id > 999) {
        printf(COLOR_RED "[ERROR] 输入无效，使用默认值 42" COLOR_RESET "\n");
        new_id = 42;
    }

    snprintf(write_buf, sizeof(write_buf), "%d\n", new_id);
    printf(COLOR_BLUE "[INFO] 写入值: %s" COLOR_RESET "\n", write_buf);

    if (write_sysfs_attr(SYSFS_DEV_ATTR, write_buf) == 0) {
        printf(COLOR_GREEN "[RESULT] 写入成功!" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "[ERROR] 写入失败!" COLOR_RESET "\n");
        printf(COLOR_YELLOW "[INFO] 请检查: 1) 模块是否加载  2) 权限是否正确" COLOR_RESET "\n");
        return;
    }

    /* 2.3 再次读取验证 */
    printf("\n");
    printf(COLOR_BLUE "[INFO] 步骤3: 再次读取 xdev_id 验证写入" COLOR_RESET "\n");
    if (read_sysfs_attr(SYSFS_DEV_ATTR, buf, sizeof(buf)) == 0) {
        printf(COLOR_GREEN "[RESULT] 当前 xdev_id = %s" COLOR_RESET "\n", buf);
        
        /* 验证值是否正确 */
        int verified_id;
        sscanf(buf, "%d", &verified_id);
        if (verified_id == new_id) {
            printf(COLOR_GREEN "[SUCCESS] 值验证通过! 写入 %d, 读取 %d" COLOR_RESET "\n", new_id, verified_id);
        } else {
            printf(COLOR_RED "[FAIL] 值不匹配! 写入 %d, 读取 %d" COLOR_RESET "\n", new_id, verified_id);
        }
    }

    printf("\n");
}

/* ==================== 测试3: 驱动属性 ==================== */
void test_driver_attribute(void)
{
    char buf[64];

    print_separator("测试3: 驱动属性 (drvname)");

    if (!file_exists(SYSFS_DRV_ATTR)) {
        printf(COLOR_YELLOW "[WARN] 驱动属性文件不存在: %s" COLOR_RESET "\n", SYSFS_DRV_ATTR);
        printf(COLOR_YELLOW "[INFO] 请先加载内核模块: insmod xdrv.ko" COLOR_RESET "\n\n");
        return;
    }

    printf(COLOR_BLUE "[INFO] 读取驱动属性: %s" COLOR_RESET "\n", SYSFS_DRV_ATTR);
    if (read_sysfs_attr(SYSFS_DRV_ATTR, buf, sizeof(buf)) == 0) {
        printf(COLOR_GREEN "[RESULT] 驱动名称: %s" COLOR_RESET "\n", buf);
    }

    printf("\n");
}

/* ==================== 测试4: 设备驱动匹配验证 ==================== */
void test_match_verification(void)
{
    char dev_buf[64];
    char drv_buf[64];
    char match_buf[64];

    print_separator("测试4: 设备与驱动匹配状态验证");

    /* 检查设备是否存在 */
    if (!file_exists(SYSFS_DEV_ATTR)) {
        printf(COLOR_YELLOW "[WARN] 设备模块未加载" COLOR_RESET "\n");
    } else {
        printf(COLOR_BLUE "[INFO] 设备 'xdev' 已注册" COLOR_RESET "\n");
    }

    /* 检查驱动是否存在 */
    if (!file_exists(SYSFS_DRV_ATTR)) {
        printf(COLOR_YELLOW "[WARN] 驱动模块未加载" COLOR_RESET "\n");
    } else {
        printf(COLOR_BLUE "[INFO] 驱动 'xdrv' 已注册" COLOR_RESET "\n");
    }

    /* 检查 match 结果 */
    if (file_exists(SYSFS_DEV_ATTR) && file_exists(SYSFS_DRV_ATTR)) {
        printf("\n");
        printf(COLOR_BLUE "[INFO] 检查内核日志中的匹配信息:" COLOR_RESET "\n");
        printf(COLOR_BLUE "[INFO] 执行: dmesg | grep 'dev & drv match'" COLOR_RESET "\n");
        printf(COLOR_GREEN "[INFO] 如果看到 'dev & drv match' 输出，说明匹配成功!" COLOR_RESET "\n");
        printf("\n");
    }
}

/* ==================== 测试5: 综合测试 - 完整流程 ==================== */
void test_comprehensive(void)
{
    print_separator("测试5: 综合测试 - 完整加载/卸载流程");

    printf(COLOR_BLUE "[INFO] 完整测试流程说明:" COLOR_RESET "\n\n");
    
    printf(COLOR_BLUE "  1. 加载模块顺序:" COLOR_RESET "\n");
    printf("     %s\n", "     insmod xbus.ko    # 注册总线");
    printf("     %s\n", "     insmod xdev.ko    # 注册设备");
    printf("     %s\n", "     insmod xdrv.ko    # 注册驱动 (自动匹配)\n");

    printf(COLOR_BLUE "  2. 查看 sysfs 属性:" COLOR_RESET "\n");
    printf("     %s\n", "     cat /sys/bus/xbus/devices/xbus_test");
    printf("     %s\n", "     cat /sys/devices/platform/xdev/xdev_id");
    printf("     %s\n", "     cat /sys/bus/xbus/drivers/xdrv/drvname\n");

    printf(COLOR_BLUE "  3. 修改设备属性:" COLOR_RESET "\n");
    printf("     %s\n", "     echo 123 > /sys/devices/platform/xdev/xdev_id\n");

    printf(COLOR_BLUE "  4. 查看匹配日志:" COLOR_RESET "\n");
    printf("     %s\n", "     dmesg | grep 'dev & drv match'\n");

    printf(COLOR_BLUE "  5. 卸载模块顺序:" COLOR_RESET "\n");
    printf("     %s\n", "     rmmod xdrv");
    printf("     %s\n", "     rmmod xdev");
    printf("     %s\n", "     rmmod xbus\n");

    printf("\n");
}

/* ==================== 主函数 ==================== */
int main(int argc, char *argv[])
{
    int choice;

    printf("\n");
    printf(COLOR_CYAN "╔══════════════════════════════════════════════════╗" COLOR_RESET "\n");
    printf(COLOR_CYAN "║                                                  ║" COLOR_RESET "\n");
    printf(COLOR_CYAN "║     Linux Device Module 用户空间测试程序         ║" COLOR_RESET "\n");
    printf(COLOR_CYAN "║                                                  ║" COLOR_RESET "\n");
    printf(COLOR_CYAN "╚══════════════════════════════════════════════════╝" COLOR_RESET "\n\n");

    printf(COLOR_YELLOW "请选择测试项目:" COLOR_RESET "\n");
    printf("  " COLOR_GREEN "1" COLOR_RESET ". " COLOR_BLUE "测试总线属性" COLOR_RESET "\n");
    printf("  " COLOR_GREEN "2" COLOR_RESET ". " COLOR_BLUE "测试设备属性 (xdev_id 读写)" COLOR_RESET "\n");
    printf("  " COLOR_GREEN "3" COLOR_RESET ". " COLOR_BLUE "测试驱动属性 (drvname)" COLOR_RESET "\n");
    printf("  " COLOR_GREEN "4" COLOR_RESET ". " COLOR_BLUE "设备驱动匹配状态验证" COLOR_RESET "\n");
    printf("  " COLOR_GREEN "5" COLOR_RESET ". " COLOR_BLUE "综合测试 - 完整流程说明" COLOR_RESET "\n");
    printf("  " COLOR_GREEN "0" COLOR_RESET ". " COLOR_RED "退出" COLOR_RESET "\n\n");
    printf("请输入选项 [0-5]: " COLOR_RESET);

    if (scanf("%d", &choice) != 1) {
        printf(COLOR_RED "[ERROR] 输入无效，程序退出!" COLOR_RESET "\n");
        return 1;
    }

    switch (choice) {
    case 1:
        test_bus_attribute();
        break;
    case 2:
        test_device_attribute();
        break;
    case 3:
        test_driver_attribute();
        break;
    case 4:
        test_match_verification();
        break;
    case 5:
        test_comprehensive();
        break;
    case 0:
        printf("\n" COLOR_YELLOW "[INFO] 程序退出!" COLOR_RESET "\n");
        return 0;
    default:
        printf(COLOR_RED "[ERROR] 无效选项! 请输入 0-5 之间的数字.\n" COLOR_RESET);
        return 1;
    }

    return 0;
}
