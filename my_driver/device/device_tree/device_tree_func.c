
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/uaccess.h>

#include <linux/of.h>
#include <linux/platform_device.h>


/*
 get_dts_info_test: get_dts_info_test{
    compatible = "get_dts_info_test";
    #address-cells = <1>;
    #size-cells = <1>;

    led@0xfdd60000{//GPIO0_  fdd60000     GPIO4_   fe770000
            compatible = "fire,led_test";
            reg = <0xfdd60000 0x00000100>;
            status = "okay";
    };
};

*/

/*****************************************************
 * 
 *                      函数声明
 * 
 ******************************************************/
static int get_dts_info_probe(struct platform_device *pdev);
static int get_dts_info_remove(struct platform_device *pdev);


/*****************************************************
 * 
 *                      变量声明
 * 
 ******************************************************/
struct device_node *led_test_device_node;   //设备树节点
struct device_node *led_device_node;        //节点
struct property *led_property;              //属性

/*匹配表: 用于driver和device匹配 - 通过设备树创建设备节点, 通过匹配表完成device和driver匹配*/
static const struct of_device_id of_gpio_leds_match[] = {
    {.compatible = "get_dts_info_test"},
    {}
};

static int size = 0;
static unsigned int out_values[18];


static struct platform_driver get_dts_info_driver = {
    .probe = get_dts_info_probe,
    .remove = get_dts_info_remove,
    .driver = {
        .name = "get_dts_info_test",
        .of_match_table = of_match_ptr(of_gpio_leds_match)
    }
};

/*****************************************************
 * 
 *                  驱动接口定义
 * 
 ******************************************************/
/**
 * @brief probe
 * @param pdev: 设备节点
 * @note 
 */
static int get_dts_info_probe(struct platform_device *pdev)
{
    int ret = 0;

    pr_info("%s\n",__func__);

    led_test_device_node = of_find_node_by_path("/get_dts_info_test");
    if (NULL == led_test_device_node)
    {
        printk("get get_dts_info_test node failed\r\n");
        return -1;
    }

    /*输出节点名*/
    printk("name: %s\r\n", led_test_device_node->name);
    printk("child name: %s\r\n", led_test_device_node->child->name);


    /*获取子节点*/
    led_device_node = of_get_next_child(led_test_device_node, NULL);
    if (NULL == led_device_node)
    {
        printk("get led node failed\r\n");
        return -1;
    }

    /*获取compatible属性值*/
    led_property = of_find_property(led_device_node, "compatible", &size);
    if (NULL == led_property)
    {
        printk("get led_property failed\r\n");
        return -1;
    }

    printk("size = : %d\r\n",size);                          //实际读取得到的长度
    printk("name: %s\r\n",led_property->name);               //输出属性名
    printk("length: %d\r\n",led_property->length);           //输出属性长度
    printk("value : %s\r\n",(char*)led_property->value);     //属性值

    /*获取reg属性值*/
    ret = of_property_read_u32_array(led_device_node, "reg", out_values, 2);
    if (ret != 0)
    {
        printk("get reg values failed\r\n");
        return -1;
    }

    printk("0x%08X ", out_values[0]);
    printk("0x%08X\r\n", out_values[1]);

    return 0;
}

/**
 * @brief remove
 * @param pdev: 设备节点
 * @note 
 */
static int get_dts_info_remove(struct platform_device *pdev)
{
    pr_info("%s\n",__func__);
	return 0;
}


/*****************************************************
 * 
 *                  模块注册
 * 
 ******************************************************/
/*等价于执行了module_init, module_exit*/
module_platform_driver(get_dts_info_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

