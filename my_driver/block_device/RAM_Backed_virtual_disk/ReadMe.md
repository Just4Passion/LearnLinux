
# RAM_Backed_virtual_disk简介

## 项目
```
内存块设备: 内存模拟磁盘
内存空间动态管理: 通过管理物理页, 在写入数据时动态申请添加物理页
数据读写: 把物理页映射到内核地址空间, 在内核地址空间上对数据进行访问
```


## 用法
1. **编译安装**
    - insmod ram_disk.ko
    - 生成设备节点
        - /dev/ram0-15
2. **路径验证**
    - cat /sys/module/ram_disk/
    - cat /sys/module/
    - cat /sys/block/ram
    - cat /proc/devices
    - modinfo ramdisk.ko
    - fdisk -l /dev/ram0
3. **文件系统格式**
    - mkfs.ext4 /dev/ram0
    - 挂载
        - mkdir /mnt/ram0
        - mount /dev/ram0 /mnt/ram0
    - 写入文件测试
        - cd /mnt/ram0
        - echo "Hello RAM Disk" > test.txt
    - 查看块设备写入是否正常
        - cat /dev/ram0: 在末尾可以看到text.txt的内容
        - 说明块设备格式化正常, 挂载正常
    - dd命令测试写入速度
        - cd /mnt/ram0
        - dd if=/dev/zero of=bigfile bs=1M count=100 status=progress



# 模块简介

## 文件系统


## mkfs.ext4
    - 参数理解
        - -b: 块大小
        - -i: 每个inode的字节数
        - -m: 保留给root的磁盘空间
        - -L: 卷标名
        - -N: 指定inode的数量
        - -E: 扩展选项
        - -O: 启用或禁用文件系统特性
        - -T: 指定预设的使用类型
        - -q: 静默模式
        - -v: 详细输出

## df命令


## dd命令




