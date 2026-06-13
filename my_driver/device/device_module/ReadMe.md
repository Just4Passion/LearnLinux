
# device_module简介

## 项目
```
了解linux驱动模型: 驱动代码 = 设备(device) + 驱动(driver + bus)
掌握设备模型框架下, 设备驱动开发的基本步骤

编程思路:
    - 编写Makefile文件
    - 声明一个总线结构体并创建一个总线xbus，实现match方法，对设备和驱动进行匹配
    - 声明一个设备结构体，挂载到我们的xbus总线中
    - 声明一个驱动结构体，挂载到xbus总线，实现probe、remove方法
    - 将总线、设备、驱动导出属性文件到用户空间。
```


# 模块简介


## 设备, 驱动, 总线, 属性(sysfs)
1. **struct device**: 设备
    - 实际挂载的设备, 绑定硬件信息
    - 接口
        - device_register()
        - device_unregister()
    - 私有接口
        - 
2. **struct device_driver**: 驱动
    - 操作设备, 访问设备的硬件信息
    - 接口
        - driver_register()
        - driver_unregister()
    - 私有接口
        - probe
        - remove

3. **struct bus_type**: 总线
    - 管理挂载的驱动和设备
    - 负责匹配注册的驱动和设备
    - 接口
        - bus_register()
        - bus_unregister()
        - bus_create_file()
        - bus_remove_file()
    - 私有接口
        - match

4. **struct attribue**: 属性
    - 作用: 属性文件是在 sysfs 中显示的文件
        - 导出设备/驱动/总线的信息
        - 提供用户空间配置接口
        - 实现用户空间与内核空间的交互
    - 设备属性: struct device_attribute
    - 驱动属性: struct driver_attribute
    - 总线属性: struct bus_attribute
    - sysfs + 属性文件, 提供了一种机制: 用户可以直接通过属性文件访问和修改设备驱动的参数
    - 接口
        - bus_create_file()
        - bus_remove_file()
        - devcie_create_file()
        - device_remove_file()
        - driver_create_file()
        - driver_remove_file()


