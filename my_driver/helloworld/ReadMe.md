
# helloworld
    - 关键概念
        - 模块
        - 模块装载和卸载
        - 模块编译
        - 模块依赖关系
        - 模块参数
        - 模块符号

## 目的: 了解模块的最小组成
### 模块源文件编写
1. **头文件**
    - #include <linux/module.h>
    - #include <linux/init.h>
    - #include <linux/kernel.h>
2. **注册和声明**
    - module_init: 注册装载时调用的初始化函数
    - module_exit: 注册卸载时调用的销毁函数
    - MODULE_LICENSE: 模块许可证
3. **模块参数**
    - module_param: module_param(参数名, 参数类型, 参数读/写权限)
    - module_param_array: module_param_array(数组名, 数组类型, 数组长, 参数读/写权限)
    - 类型: byte, short, ushort, int, uint, long, ulong, charp, bool, invbool
    - 权限: 
4. **导出符号**
    - EXPORT_SYMBOL: 导出符号
    - 外部使用
        - 使用extern声明的方式导出符号

### 构建脚本编写
```
KERNEL_DIR=../../kernel

ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-

export ARCH CROSS_COMPILE

obj-m := helloworld.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) modules

.PHONE:clean

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) clean

```
1. **指定内核源码目录**: -C
    - -C $(KERNEL_DIR) 表示在执行 make 命令前，先切换到 $(KERNEL_DIR) 目录
        - 内核模块的编译依赖内核源码树中的头文件和 Makefile 规则
            - 可以利用内核的编译系统（Kbuild）
            - 自动包含正确的头文件路径
            - 使用与内核一致的编译选项和版本信息
2. **指定模块源码目录**: M=
    - M=$(CURDIR)：告诉内核的 Makefile，要编译的模块源码在当前目录（$(CURDIR) 是你执行 make 命令的目录）
3. **指定编译目标**: modules
    - modules，表示编译内核模块
4. **芯片架构和交叉编译链**
    - ARCH
    - CROSS_COMPILE



## 使用方法
1. **修改Makefile**
    - 根据用户虚拟机版本，修改源码中的模块依赖路径，也就是内核源码路径
2. **编译**
    - make: 在目录下执行make指令，自动编译生成模块
    - make clean: 清空生成的文件
3. **使用**
    - modinfo: 显示模块信息
        - modinfo helloworld.ko
    - insmod: 加载模块
        - insmod helloworld
        - insmod helloworld author_name=MouMouMou author_age=33
    - rmmod: 卸载模块
        - rmmod helloworld
    - lsmod: 查看模块
        - lsmod | grep helloworld
    - modprobe: 装载/卸载模块. 自动检查依赖
        - modprobe helloworld: 装载
        - modprobe -r helloworld: 卸载
4. **内核打印**
    - dmesg: 执行命令，查看内核打印


## 模块与sysfs的关系: 内核虚拟文件系统
```
sys
    block: 系统所有块设备的符号链接, 指向/sys/devices中的实际设备
    bus: 内核支持的各种总线类型, 每种总线下显示其设备和驱动
    class: 按功能分类设备的高级视图
    dev: 包含设备的字符设备和块设备的主次设备号
    devices: 全局设备树
    firmware: 用于固件相关的属性和操作接口
    fs: 包含文件系统相关的内核参数和属性
    kernel: 内核核心子系统和参数
    module: 加载的内核模块信息
    power: 电源管理接口
```
1. **概念**
    - 内核虚拟文件系统，仅存在于内存中
    - 核心作用
        - 将内核中的设备、驱动、总线、资源等对象的属性以“文件”的形式暴露给用户层
        - 实现用户层与内核层的双向交互，用户层可读取文件获取内核信息，也可写入文件修改内核配置
2. **/sys/module/**
    - 显示已经加载的内核模块
    - 当helloworld加载成功时，可以看到/sys/module/helloworld目录
        - coresize
        - holders/: 依赖当前模块的其他模块列表
        - initsize
        - initstate
        - notes/: ELF注释段信息
        - parameters/: 模块参数
        - refcnt: 模块引用计数
        - sections/: 模块各段的内存地址信息
        - srcversion
        - taint: 内核污染状态标志
        - uevent: 模块的uevent属性


# 补充
## 模块依赖关系: /lib/modules/$(Kernel_Version)/modules.dep
```
build: 指向当前正在运行的内核源代码的符号链接
kernel: 包含编译后的内核模块文件（.ko）
modules.alias: 定义模块别名的文件
modules.alias.bin: 模块别名文件的二进制缓存版本
modules.builtin: 列出了由内核构建的模块（静态连接在内核中）
modules.builtin.bin: 由内核构建的模块列表的二进制缓存版本
modules.dep: 列出了模块之间的依赖关系
modules.dep.bin: 模块依赖关系文件的二进制缓存版本
modules.devname: 包含了每个模块设备的名称
modules.order: 定义模块加载顺序的文件
modules.symbols: 保存导出的符号信息
modules.symbols.bin: 导出的符号信息的二进制缓存版本
modules.softdep: 包含模块软依赖关系的文件
```
1. **depmod**
    - 用于分析内核模块的依赖关系，生成模块依赖信息文件，供 modprobe 等工具使用
2. **depmod的使用方法**
    - 安装新内核后
        - depmod $(uanme -r): 为新内核生成模块依赖关系
    - 手动编译安装模块后
        - cp my_driver.ko /lib/modules/$(uname -r)/extra/
        - depmod -a

## 参数权限
```
// 用户权限
#define S_IRUSR  00400  // 用户读
#define S_IWUSR  00200  // 用户写
#define S_IXUSR  00100  // 用户执行

// 组权限
#define S_IRGRP  00040  // 组读
#define S_IWGRP  00020  // 组写
#define S_IXGRP  00010  // 组执行

// 其他用户权限
#define S_IROTH  00004  // 其他读
#define S_IWOTH  00002  // 其他写
#define S_IXOTH  00001  // 其他执行
```
