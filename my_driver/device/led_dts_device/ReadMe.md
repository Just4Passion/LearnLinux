

# led_dts_device简介

## 项目简介
```
介绍什么是DTS, 什么是DTB
内核如何使用DTB
驱动如何访问DTB
```

## 问题
1. **iowrite32和writel**
	- iowrite32和writel都是用于写入32位数据到I/O内存的函数，区别主要体现在字节序处理和使用场景上
		- writel: 总是以小端序写入
		- iowrite32: 以CPU原生序写入
	- 小端序和大端序
		- 小端序: 低位字节存储在低地址, 高位字节存储在高地址
		- 大端序: 高位字节存储在低地址, 低位字节存储在低地址


# 模块介绍


## 设备模型: 驱动代码分成了两部分 - 设备 + 驱动
1. **背景**
    - 直接把硬件信息写入到驱动中, 当硬件信息更改时, 驱动也需要更改
    - 把驱动代码分成两部分: 设备 + 驱动
    - 设备与硬件信息绑定; 驱动负责使用这些设备提供的硬件资源
2. **设备模型**
    - 设备: 挂载在某个总线上的物理设备
    - 驱动: 与特定设备相关的软件, 负责初始化该设备以及提供一些操作该设备的操作方式
    - 总线: 负责管理挂载对应总线的设备以及驱动
    - 类: 对具有相同功能的设备，归纳到一种类别，进行分类管理

## DTS, DTB：述一个硬件平台的硬件资源
1. **是什么**
	- DTS: Device Tree Source
		- 描述硬件平台的硬件资源
		- 以"树状"结构描述硬件资源
		- 可以使用#include包含, 被引用
	- DTB: DTS的二进制形式
		- 设备树可以被bootloader(uboot)传递到内核， 内核可以从设备树中获取硬件信息
	- DTC: 设备树源码编译工具
		- dtc -i dts -o dtb intput.dts output.dtb
		- dtc -i dts -o dtbo intput.dts output.dtbo
### 文本内容
```
dts-v1/;   //必要的DTS文件版本说明

#include "example.dtsi"  //包含头文件，可以是.dtsi、.dts、.h文件等

/ { // 根节点
    node1-name@0x10000000 {   //节点1，名称是“node1-name”，单元地址和reg属性的第一个地址一致
        compatible = "xxx, xxxx";
        reg = <0x10000000 0x1000>; // reg属性与单元地址匹配
        a-string-property = "A string"; // 字符串类型属性
        a-string-list-property = "first string", "second string";
        a-byte-data-property = [0x00 0x13 0x24 0x36];

        label: child-node1 {    // 节点1的子节点1，“label”是标签
            first-child-property; // 空属性
            second-child-property = <1>;
            a-string-property = "Hello, world";
        };

        child-node2 {   // 子节点2

        };
    };

    node2-name {    // 节点2，无单元地址，无需reg属性
        an-empty-property;
        a-cell-property = <1 2 3 4>;    // 32位无符号整数数组属性

        child-node1 {   // 节点2的子节点
            my-cousin = <&cousin>; // 引用其他节点的标签
        };
    };
};
```
1. **节点名称**: node-name@unit-address
	- node1-name@unit-address
		- @, 分隔符
		- unit-address, 用于指定单元地址, 值和"reg"属性的第一个地址值一致
2. **节点标签**: label: node-name@unit-address
	- cpu0: cpu@0
		- "cpu0"是节点标签
		- 节点标签是节点名的简写
		- 在其他位置引用时, 可以使用节点标签来向该节点追加内容
3. **节点路径**
	- 从根节点到所需节点的完整路径
4. **节点属性**
	- compatible属性: 一个或多个字符串组成, 使用","隔开
		- compatible = "embedfire,lubancat2", "rockchip,rk3568";
	- model属性: 用于指定设备的制造商和型号
		- model = "EmbedFire LubanCat2 HDMI+MIPI";
	- status属性: 指示设备的操作状态
		- status = "disabled";
		- status = "okay";
	- #address-cells和#size-cells: 用于设置子节点的“reg”属性的“书写格式”
		- reg = <cells cells cells>
		- 每个cells都由address和size组成
		- #address-cells指定了地址字段所占的长度, #size-cells指定了大小字段所占的长度
			- #address-cells = 2, #size-cells = 1: reg = <address address size address address size>
			- #address-cells = 1, #size-cells = 1: reg = <address address>
	- reg属性: 地址、长度数据对
		- 描述设备资源在其总线定义的地址空间内的地址, 通常情况用于表示一块寄存器的起始地址和长度
	- ranges: 地址映射/转换表. 任意数量的<子地址、父地址、地址长度>编码
		- ranges = <0x0 0x10 0x20>: 将子地址0x0~(0x0+0x20)的地址空间映射到父地址0x10~(0x10+0x20)的地址空间
5. **特殊节点**
	- 追加/修改节点内容: &label_name
		- @cpu0 {};
	- aliases节点: 为其他节点起一个别名
		- `aliases {serial0 = &uart0; }: serial0是别名`
	- chosen节点: 给内核传递参数
		- `chosen { bootargs = "key=value,key=value key=value key=value"; }`


## /proc/device-tree




## 绑定信息文档: Linux 源码目录/Documentation/devicetree/bindings
```
目录中包含了SOC上不同硬件的节点描述信息
当需要在设备树中添加新的节点的时候，就需要阅读这里的文档
```

## 设备操作函数: OF函数
1. **struct device_node**
```
struct device_node {
	const char *name;
	phandle phandle;
	const char *full_name;				//节点名
	struct fwnode_handle fwnode;

	struct	property *properties;		//属性
	struct	property *deadprops;	/* removed properties */
	struct	device_node *parent;		//父节点
	struct	device_node *child;			//子节点
	struct	device_node *sibling;		//兄弟节点
#if defined(CONFIG_OF_KOBJ)
	struct	kobject kobj;
#endif
	unsigned long _flags;
	void	*data;
#if defined(CONFIG_SPARC)
	unsigned int unique_id;
	struct of_irq_controller *irq_trans;
#endif
};
```
2. **查找节点**
	- of_find_node_by_name
		- struct device_node *of_find_node_by_name(struct device_node *from, const char *name)
		- NULL表示从根节点开始查找
		- name是节点名
	- of_find_node_by_type
		- struct device_node *of_find_node_by_type(struct device_node *from,const char *type)
		- 根据类型查找
		- type是device_type的值, 比如"cpu"
	- of_find_compatible_node 
		- struct device_node *of_find_compatible_node(struct device_node *from,const char *type, const char *compatible)
		- 根据device_type和compatible属性值查找
	- of_find_matching_node_and_match
		- static inline struct device_node *of_find_matching_node_and_match(struct device_node *from, const struct of_device_id *matches, const struct of_device_id **match)
		- 通过struct of_device_id匹配表查找: struct of_device_id中包含name, type, compatible, data(该节点的其他属性)
    - of_find_node_by_path: 通过节点路径查找
        - inline struct device_node *of_find_node_by_path(const char *path)
3. **查找父/子节点**
	- of_get_parent
		- struct device_node *of_get_parent(const struct device_node *node)
	- of_get_next_child
		- struct device_node *of_get_next_child(const struct device_node *node, struct device_node *prev)
4. **提取属性值**
	- of_find_property: 查找属性
		- struct property *of_find_property(const struct device_node *np,const char *name,int *lenp)
	- of_property_count_elems_of_size: 获取属性中元素的数量
		- int of_property_count_elems_of_size(const struct device_node *np, const char *propname, int elem_size)
	- of_property_read_u32_index: 从属性中获取指定标号的u323类型数据
		- int of_property_read_u32_index(const struct device_node *np,const char *propname,u32 index, u32 *out_value)
	- of_property_read_u8_array: 读取属性中的u8类型的数组数据
		- int of_property_read_u8_array(const struct device_node *np, const char *propname, u8 *out_values, size_t sz)
		- 还有u16, u32, u64等函数
	- of_property_read_u8: 读取属性中只有一个整型值的数据
		- int of_property_read_u8(const struct device_node *np, const char *propname, u8 *out_value)
		- 还有u16, u32, u64等函数
	- of_property_read_string: 读取字符串
		- int of_property_read_string(struct device_node *np, const char *propname, const char **out_string)
		- 读取属性中字符串值
	- of_n_addr_cells: 获取#address-cells属性值
		- int of_n_addr_cells(struct device_node *np)
	- of_n_size_cells: 获取#size-cells属性值
		- int of_n_size_cells(struct device_node *np)

5. **内存映射**
	- of_iomap: 获取寄存器的值, 映射虚拟地址
		- void __iomem *of_iomap(struct device_node *np, int index)
		- reg通常包含多段, index用于指定映射那一段, 标号从0开始


## 平台总线match逻辑
```
1. driver_override 强制匹配
   ↓ 失败
2. OF (Device Tree) 风格匹配
   ↓ 失败
3. ACPI 风格匹配
   ↓ 失败
4. ID table 匹配
   ↓ 失败
5. 名称直接匹配（fallback）
```



