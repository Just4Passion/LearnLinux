
# dynamic_device_tree简介


## 项目
```
介绍动态设备树的编写方式
动态设备树是为了适应变化
动态设备树的叠加发生在uboot中
```


## 插件格式
1. **格式1**: fragment, target, __overlay__
```
// 插件格式：必须有 /plugin/ 标记
/dts-v1/;
/plugin/;  // 关键：标记为插件

/ {
    compatible = "vendor,base-board";
    
    // 使用 fragment 描述修改
    fragment@0 {
        target = <&i2c0>;  // 目标节点引用
        
        __overlay__ {
            status = "okay";
            
            sensor@48 {
                compatible = "vendor,sensor";
                reg = <0x48>;
            };
        };
    };
    
    fragment@1 {
        target-path = "/soc/spi@40010000";  // 使用路径
        
        __overlay__ {
            status = "okay";
            
            display@0 {
                compatible = "vendor,display";
                reg = <0>;
            };
        };
    };
};
```

2. **格式2**: &label
```
// 新版本支持更简洁的语法（dtc 1.5+）
/dts-v1/;
/plugin/;

// 直接引用标签，无需 fragment
&i2c0 {
    status = "okay";
    
    sensor@48 {
        compatible = "vendor,sensor";
        reg = <0x48>;
    };
};

&spi0 {
    status = "okay";
    
    flash@0 {
        compatible = "jedec,spi-nor";
        reg = <0>;
    };
};

// 使用路径
&{/soc/uart@40020000} {
    status = "okay";
};
```
