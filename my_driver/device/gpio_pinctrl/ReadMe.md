
# gpio_pinctrl简介

## 项目
```
pinctrl子系统：专注于引脚功能复用、电气属性配置
    - pinctrl 子系统主要工作内容如下：
        - 获取设备树中 pin 信息。
        - 根据获取到的 pin 信息来设置 pin 的复用功能
        - 根据获取到的 pin 信息来设置 pin 的电气特性，如驱动能力。
    - 对于我们使用者来讲，只需要在设备树里面设置好某个 pin 的相关属性即可
        - 其他的初始化工作均由 pinctrl 子系统来完成，pinctrl 子系统源码目录为 drivers/pinctrl


gpio子系统：基于pinctrl配置结果，实现通用输入输出的电平控制与方向管理


1. 工作流程
    设备驱动 -> 需要使用引脚 -> pinctrl：配置引脚功能
                                    |
                                    |
                ——————————————————————————————————————————
                |                   |                     |
            UART控制器接管      I2C控制器接管         GPIO子系统接管
                                                          |
                                                ————————————————————
                                                |         |         |
                                        设置输入/输出   设置中断    读写电平
```




# 模块简介

## pinctrl子系统
    - 功能: 负责引脚复用和电气特性配置（上下拉、驱动能力等）
1. **核心数据结构**
```
pinctrl_dev
├── pinctrl_desc
│   ├── pins[]          (每个引脚的静态描述)
│   ├── pinmux_ops      (引脚复用操作集)
│   └── pinconf_ops     (引脚配置操作集)
├── pin_function_tree   (功能->引脚组映射)
└── pin_group_tree      (引脚组->引脚列表映射)
```
2. **设备树描述**
```
pinctrl: pinctrl@01c20800 {
    compatible = "allwinner,sunxi-pinctrl";
    reg = <0x01c20800 0x400>;
    
    uart0_pins: uart0-pins {
        pins = "PA4", "PA5";
        function = "uart0";
    };
    
    led_pins: led-pins {
        pins = "PA6";
        function = "gpio_out";
        bias-pull-up;
    };
};
```
3. **引脚状态切换流程**
```
pinctrl_get(dev);
pinctrl_select_state(p, pinctrl->default_state);

内核执行流程：
    解析设备树中的 pinctrl-0、pinctrl-names。
    找到对应的 pinctrl_state。
    遍历该状态包含的所有设置（settings）。
    对每个设置：
        调用 pinmux_ops->set_mux() 配置复用功能。
        调用 pinconf_ops->pin_config_set() 配置上下拉、驱动强度等
```

## gpio子系统
    - 功能: 引脚的输入/输出逻辑值（高/低电平、中断等）
1. **核心数据结构**
```
gpio_device
├── gpio_chip
│   ├── base           (该芯片的起始GPIO编号)
│   ├── ngpio          (支持的GPIO数量)
│   ├── direction_input()
│   ├── direction_output()
│   ├── get()
│   ├── set()
│   └── set_config()   (可选，配置上下拉等)
├── descs[]            (gpio_desc数组)
│   └── gpio_desc
│       ├── flag       (使用标志：REQUESTED, ACTIVE_LOW等)
│       └── label      (使用者标签)
```
2. **gpio请求流程**: gpiod_get(dev, "led", GPIOD_OUT_LOW)
    - 设备树解析
```
led {
    compatible = "gpio-leds";
    gpios = <&pinctrl 6 GPIO_ACTIVE_LOW>;
};
```
    - 查找gpio_desc: 通过 of_get_named_gpiod_flags() → gpiochip_find() → 找到对应 gpio_chip 和硬件偏移
    - pinctrl自动切换: 如果 gpio_chip 设置了 .request() 回调，通常它会调用pinctrl_gpio_request(gpio);
    - 方向设置: gpiod_direction_output(desc, 0)
3. **中断支持**
```
// gpio_chip 中设置
chip->to_irq = my_gpio_to_irq;

// 内部实现
int my_gpio_to_irq(struct gpio_chip *chip, unsigned offset) {
    return irq_domain_create_mapping(chip->irq_domain, offset);
}
```
    - 创建 irq_domain 将GPIO偏移映射到Linux IRQ号。
    - 使用 generic_handle_irq() 触发中断处理。
    - 支持边沿/电平触发，通过寄存器配置（如GPIO_CFG的边沿检测位）。


## 当存在多个灯时: 编写驱动需要遍历子节点
1. **设备树**
```
leds {
    compatible = "gpio-leds";
    
    led0: led-0 {
        label = "led0";
        gpios = <&pinctrl 6 GPIO_ACTIVE_LOW>;
        default-state = "on";
    };
    
    led1: led-1 {
        label = "led1";
        gpios = <&pinctrl 7 GPIO_ACTIVE_HIGH>;
        default-state = "off";
    };
    
    led2: led-2 {
        label = "led2";
        gpios = <&pinctrl 8 GPIO_ACTIVE_HIGH>;
        linux,default-trigger = "heartbeat";
    };
};
```
2. **probe执行流程**
    - 只匹配父节点: { .compatible = "gpio-leds", }
    - 遍历所有子节点: for_each_available_child_of_node(np, child)
```
// drivers/leds/leds-gpio.c
static const struct of_device_id of_gpio_leds_match[] = {
    { .compatible = "gpio-leds", },
    {},
};
MODULE_DEVICE_TABLE(of, of_gpio_leds_match);

static struct platform_driver gpio_led_driver = {
    .probe = gpio_led_probe,  // 只调用一次！
    .driver = {
        .name = "leds-gpio",
        .of_match_table = of_gpio_leds_match,
    },
};
module_platform_driver(gpio_led_driver);



// drivers/leds/leds-gpio.c (简化版)
static int gpio_led_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_node *np = dev_of_node(dev);
    struct device_node *child;
    struct gpio_led_data *led_dat;
    int ret, count = 0;

    /* 遍历所有子节点 */
    for_each_available_child_of_node(np, child) {
        struct gpio_led led = {};
        const char *state = NULL;
        
        /* 从每个子节点解析GPIO信息 */
        led.gpiod = devm_fwnode_get_gpiod_from_child(
            dev, NULL, &child->fwnode,
            GPIOD_ASIS, child->name);
        
        if (IS_ERR(led.gpiod)) {
            dev_err(dev, "Failed to get GPIO for %s\n", child->name);
            continue;
        }
        
        /* 解析其他属性 */
        of_property_read_string(child, "label", &led.name);
        of_property_read_string(child, "linux,default-trigger", 
                               &led.default_trigger);
        of_property_read_string(child, "default-state", &state);
        
        if (!strcmp(state, "on"))
            led.default_state = LEDS_GPIO_DEFSTATE_ON;
        else if (!strcmp(state, "off"))
            led.default_state = LEDS_GPIO_DEFSTATE_OFF;
        else
            led.default_state = LEDS_GPIO_DEFSTATE_KEEP;
        
        /* 为每个子节点创建LED设备 */
        ret = create_gpio_led(&led, led_dat, dev, child, NULL);
        if (ret < 0) {
            dev_err(dev, "Failed to create LED %s\n", child->name);
            continue;
        }
        
        count++;
    }
    
    platform_set_drvdata(pdev, led_dat);
    dev_info(dev, "Registered %d GPIO LEDs\n", count);
    
    return 0;
}
```
3. **创建led实例**: create_gpio_led
```
// drivers/leds/leds-gpio.c
static int create_gpio_led(const struct gpio_led *template,
                          struct gpio_led_data *led_dat,
                          struct device *parent,
                          struct device_node *np,
                          struct gpio_desc *gpiod)
{
    struct led_init_data init_data = {};
    int ret;

    /* 分配每个LED的私有数据 */
    led_dat = devm_kzalloc(parent, sizeof(*led_dat), GFP_KERNEL);
    if (!led_dat)
        return -ENOMEM;

    /* 初始化LED数据结构 */
    led_dat->gpiod = template->gpiod;
    led_dat->cdev.name = template->name;
    led_dat->cdev.brightness_set = gpio_led_set;
    led_dat->cdev.brightness_set_blocking = gpio_led_set_blocking;
    led_dat->cdev.blink_set = gpio_led_blink_set;
    
    /* 设置默认触发器 */
    if (template->default_trigger) {
        led_dat->cdev.default_trigger = template->default_trigger;
    }

    /* 注册到LED子系统 */
    init_data.fwnode = of_fwnode_handle(np);
    init_data.devicename = template->name;
    init_data.default_label = ":";
    
    ret = devm_led_classdev_register_ext(parent, &led_dat->cdev,
                                         &init_data);
    if (ret < 0) {
        dev_err(parent, "Failed to register LED %s\n", 
                template->name);
        return ret;
    }

    /* 设置初始状态 */
    if (template->default_state == LEDS_GPIO_DEFSTATE_ON)
        gpio_led_set(&led_dat->cdev, LED_FULL);
    else if (template->default_state == LEDS_GPIO_DEFSTATE_OFF)
        gpio_led_set(&led_dat->cdev, LED_OFF);

    /* 设置GPIO方向 */
    ret = gpiod_direction_output(led_dat->gpiod, 
        template->default_state == LEDS_GPIO_DEFSTATE_ON);
    
    return ret;
}
```







