
#include <linux/module.h>
#include <linux/init.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#include <linux/slab.h>
#include <linux/uaccess.h>


#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/regmap.h>


#include "oled_spi.h"

/*****************************************************
 * 
 *                  模块简介
 * spi_master, spi_controller: 这个与特定的SOC有关
 * spi_driver: 驱动框架, 负责初始化设备
 * spi_device: 具体的设备
 * 
 * 传输数据
 *      spi_transfer
 *      spi_message
 *
 ******************************************************/



/*****************************************************
 *                  宏定义
 ******************************************************/
#define SPI_OLED_NAME "spi_oled"
#define SPI_OLED_NUM    1


/*****************************************************
 *                  类型定义
 ******************************************************/
struct spi_oled_device {
    /*字符设备*/
    dev_t devno;
    struct cdev cdev;
    struct device *pdev;

    /*spi设备*/
    struct spi_device *pspi_dev;

    /*寄存器映射*/
    struct regmap *pregmap;

    /*资源*/
    //struct device_node *poled_node;
    int oled_dc_ctrl_pinno; //DC控制引脚

};


/*****************************************************
 *                  函数声明
 ******************************************************/
static int spi_oled_open(struct inode *node, struct file *filep);
static int spi_oled_release(struct inode *node, struct file *filep);
static ssize_t spi_oled_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos);
static ssize_t spi_oled_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos);
//static long spi_oled_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);


static int oled_regmap_spi_write(void *context, unsigned int reg, unsigned int val);


static int spi_oled_probe(struct spi_device *pdev);
static int spi_oled_remove(struct spi_device *pdev);


/*****************************************************
 *                  全局变量
 ******************************************************/
/*显示屏初始化序列*/
u8 oled_init_data[] = {
    0xae, 0xae, 0x00, 0x10, 0x40,
	0x81, 0xcf, 0xa1, 0xc8, 0xa6,
	0xa8, 0x3f, 0xd3, 0x00, 0xd5,
	0x80, 0xd9, 0xf1, 0xda, 0x12,
	0xdb, 0x40, 0x20, 0x02, 0x8d,
	0x14, 0xa4, 0xa6, 0xaf
};


/*字符设备操作函数*/
static struct file_operations spi_oled_fops = {
    .owner = THIS_MODULE,
    .open = spi_oled_open,
    .release = spi_oled_release,
    .write = spi_oled_write,
    .read = spi_oled_read,
    .unlocked_ioctl = NULL
};

/*设备信息*/
static struct spi_oled_device *pspi_oled_dev = NULL;
static struct class *pspi_oled_class = NULL;

/*regmap配置信息*/
static const struct regmap_config oled_regmap_config = {
    .val_bits = 8,                      // 寄存器数据位宽：8位
    .can_multi_write = true,            // 支持多字节批量写入
    .reg_write = oled_regmap_spi_write, // 绑定自定义SPI写入
};


/*板载信息匹配*/
static const struct spi_device_id spi_oled_device_ids[] = {
    {.name = "fire,spi_oled", .driver_data = 0},
    {}
};

/*设备树匹配*/
static const struct of_device_id spi_oled_match_ids[] = {
    {.compatible = "fire,spi_oled"},
    {}
};

/*SPI Driver*/
struct spi_driver spi_oled_driver = {
    .probe = spi_oled_probe,
    .remove = spi_oled_remove,
    .driver = {
        .name = "spi_oled",
        .of_match_table = spi_oled_match_ids,
    },
    .id_table = spi_oled_device_ids
};

/*****************************************************
 *                  辅助函数
 ******************************************************/
/**
 * @brief 自定义Regmap SPI写入回调, 仅发送数据, 不发送地址
 * @param pspi_dev: spi设备
 * @param command: 命令
 * @note 
 */
static int oled_regmap_spi_write(void *context, unsigned int reg, unsigned int val)
{
    struct spi_device *spi = context;
    u8 data = val;
    // 只发1字节纯数据，匹配SSD1306 SPI协议
    return spi_write(spi, &data, 1);
}


/**
 * @brief 向oled发送一个命令
 * @param pspi_dev: spi设备
 * @param command: 命令
 * @note 
 */
static int oled_send_command(struct spi_device *pspi_dev, u8 command)
{
    int ret = 0;

    struct spi_oled_device *pspi_oled = spi_get_drvdata(pspi_dev);
    if (NULL == pspi_oled)
    {
        dev_err(&pspi_dev->dev, "spi drvdata is null\r\n");
        return -1;
    }
    
    /*设置DC引脚为低电平*/
    gpio_direction_output(pspi_oled->oled_dc_ctrl_pinno, 0);

    /*regmap单字节写入命令*/
    ret = regmap_write(pspi_oled->pregmap, 0x00, command);
    if (ret)
    {
        dev_err(&pspi_dev->dev, "spi regmap write cmd failed\r\n");
        return ret;
    }

    /*设置DC引脚为高电平*/
    gpio_direction_output(pspi_oled->oled_dc_ctrl_pinno, 1);

    return 0;
}

/**
 * @brief 向oled发送一组命令
 * @param pspi_dev: spi设备
 * @param command: 命令
 * @param length: 总长度
 * @note 
 */
static int oled_send_commands(struct spi_device *pspi_dev, u8 *commands, u16 length)
{
	int ret = 0;
	int index ;
	
	for(index = 0; index < length; index++)
	{
		ret = oled_send_command(pspi_dev, *(commands++));
		if(ret < 0)
        {
            return -1;
        }
	}
	return 0;
}


/**
 * @brief 向oled发送一个数据
 * @param pspi_dev: spi设备
 * @param data: 数据
 * @note 
 */
static int oled_send_one_u8(struct spi_device *pspi_dev, u8 data)
{
    int ret = 0;

    struct spi_oled_device *pspi_oled = spi_get_drvdata(pspi_dev);
    if (NULL == pspi_oled)
    {
        dev_err(&pspi_dev->dev, "spi drvdata is null\r\n");
        return -1;
    }
    
    /*设置DC引脚为高电平*/
    gpio_direction_output(pspi_oled->oled_dc_ctrl_pinno, 1);

    /*regmap单字节写入命令*/
    ret = regmap_write(pspi_oled->pregmap, 0x00, data);
    if (ret)
    {
        dev_err(&pspi_dev->dev, "spi regmap write cmd failed\r\n");
        return ret;
    }

    return 0;
}

/**
 * @brief 向oled发送一组数据
 * @param pspi_dev: spi设备
 * @param data: 数据
 * @param length: 总长度
 * @note 
 */
static int oled_send_data(struct spi_device *pspi_dev, u8 *data, u16 length)
{
	int ret = 0;

    struct spi_oled_device *pspi_oled = spi_get_drvdata(pspi_dev);
    if (NULL == pspi_oled)
    {
        dev_err(&pspi_dev->dev, "spi drvdata is null\r\n");
        return -1;
    }
    
    /*设置DC引脚为高电平*/
    gpio_direction_output(pspi_oled->oled_dc_ctrl_pinno, 1);

    /*regmap批量发送*/
    ret = regmap_bulk_write(pspi_oled->pregmap, 0x00, data, length);
    if (ret) {
        dev_err(&pspi_dev->dev, "spi regmap bulk write cmd failed\r\n");
        return ret;
    }

    return 0;
}

/**
 * @brief 填充OLED显示屏
 * @param pspi_dev: spi设备
 * @param data: 数据
 * @note 
 */
void oled_fill(struct spi_device *pspi_dev, unsigned char bmp_dat)
{

    u8 y, x;
	for (y = 0; y < 8; y++)
	{
		oled_send_command(pspi_dev, 0xb0 + y);
		oled_send_command(pspi_dev, 0x01);
		oled_send_command(pspi_dev, 0x10);
		for (x = 0; x < 128; x++)
		{
			oled_send_one_u8(pspi_dev, bmp_dat);
		}
	}
}

/**
 * @brief 向 oled 发送要显示的数据，x, y 指定显示的起始位置，支持自动换行
 * @param display_buffer: 带显示的数据
 * @param x: 起始x坐标
 * @param y: 起始y坐标
 * @param length: 数据总长度
 * @note 
 */
static int oled_display_buffer(struct spi_device *pspi_dev, u8 *display_buffer, u8 x, u8 y, u16 length)
{
	u16 index = 0;
	int error = 0;

	do
	{
		/*设置写入的起始坐标*/
		error += oled_send_command(pspi_dev, 0xb0 + y);
		error += oled_send_command(pspi_dev, ((x & 0xf0) >> 4) | 0x10);
		error += oled_send_command(pspi_dev, (x & 0x0f) | 0x01);

		if (length > (X_WIDTH - x))
		{
			error += oled_send_data(pspi_dev, display_buffer + index, X_WIDTH - x);
			length -= (X_WIDTH - x);
			index += (X_WIDTH - x);
			x = 0;
			y++;
		}
		else
		{
			error += oled_send_data(pspi_dev, display_buffer + index, length);
			index += length;
			// x += length;
			length = 0;
		}

	} while (length > 0);

	if (error != 0)
	{
		/*发送错误*/
		printk("oled_display_buffer error! %d \n",error);
		return -1;
	}
	return index;
}

/**
 * @brief oled初始化
 * @note 
 */
void oled_init(struct spi_device *pspi_dev)
{
    /*初始化*/
    oled_send_commands(pspi_dev, oled_init_data, sizeof(oled_init_data));

    /*清屏*/
    oled_fill(pspi_dev, 0x00);
}


/*****************************************************
 *                  字符设备操作
 ******************************************************/
static int spi_oled_open(struct inode *node, struct file *filep)
{
    /*初始化显示屏
    显示屏作为一种共享资源. 
    应用在获取显示屏的句柄后, 需要如何操作. 因该只让前台操作吗?*/
    struct spi_oled_device *pdev = container_of(node->i_cdev, struct spi_oled_device, cdev);
    
    filep->private_data = pdev;

    /*初始化*/
    oled_init(pdev->pspi_dev);
    
    return 0;
}

static int spi_oled_release(struct inode *node, struct file *filep)
{
    return 0;
}

static ssize_t spi_oled_write(struct file *filep, const char __user *buf, size_t count, loff_t *ppos)
{
    int copy_number = 0;
    struct spi_oled_device *pdev = filep->private_data;

    /*申请内存*/
	oled_display_struct *write_data;
	write_data = (oled_display_struct*)kzalloc(count, GFP_KERNEL);

	copy_number = copy_from_user(write_data, buf, count);
    /*绘制*/
	oled_display_buffer(pdev->pspi_dev, write_data->display_buffer, 
        write_data->x, write_data->y, write_data->length);

	/*释放内存*/
	kfree(write_data);
    return 0;
}

static ssize_t spi_oled_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{

    return 0;
}

/*****************************************************
 *                  SPI驱动操作
 ******************************************************/
static int spi_oled_probe(struct spi_device *pspi_dev)
{
    /********************************************
     *  这里没有使用循环遍历, 认为它只有一个oled
     *  如果存在多个, 匹配的compatible节点下, 会有多一个oled
     */
    int ret = 0;
    /*获取硬件信息, 对设备进行初始化*/
    struct device_node *node = pspi_dev->dev.of_node;

    printk("spi oled match successed\r\n");

    /*申请资源*/
    pspi_oled_dev = devm_kzalloc(&pspi_dev->dev, sizeof(struct spi_oled_device), GFP_KERNEL);
    if (IS_ERR(pspi_oled_dev))
    {
        ret = PTR_ERR(pspi_oled_dev);
        dev_err(&pspi_dev->dev, "devm_kzalloc failed\r\n");
        return ret;
    }

    /********************************
     * 
     *          字符设备注册
     * 
     ********************************/
    ret = alloc_chrdev_region(&pspi_oled_dev->devno, 0, SPI_OLED_NUM, SPI_OLED_NAME);
    if (ret < 0)
    {
        printk("fialed to alloc oled_devno\r\n");
        return ret;
    }

    /*初始化*/
    cdev_init(&pspi_oled_dev->cdev, &spi_oled_fops);
    pspi_oled_dev->cdev.owner = THIS_MODULE;
    /*添加设备*/
    ret = cdev_add(&pspi_oled_dev->cdev, pspi_oled_dev->devno, 1);
    if (ret < 0)
    {
        printk("failed to add cdev\r\n");
        goto add_err;
    }
    /*创建设备类*/
    pspi_oled_class = class_create(THIS_MODULE, "spi_oled");
    /*创建设备节点*/
    pspi_oled_dev->pdev = device_create(pspi_oled_class, NULL, pspi_oled_dev->devno, NULL, "spi_oled");


    /********************************
     * 
     *          硬件资源初始化
     * 
     ********************************/
    /*dc资源
    spi_oled@0 {
        compatible = "fire,spi_oled";
        ...
        dc_control_pin = <&gpio3 RK_PA7 GPIO_ACTIVE_HIGH>;
        ...
    };
    */
    pspi_oled_dev->oled_dc_ctrl_pinno = of_get_named_gpio(node, "dc_control_pin", 0);
    gpio_request(pspi_oled_dev->oled_dc_ctrl_pinno, "dc_control_pin");
    gpio_direction_output(pspi_oled_dev->oled_dc_ctrl_pinno, 1);

    /*初始化SPI*/
    pspi_oled_dev->pspi_dev = pspi_dev;
    pspi_oled_dev->pspi_dev->mode = SPI_MODE_0;
    pspi_oled_dev->pspi_dev->max_speed_hz = 2000000;
    spi_setup(pspi_oled_dev->pspi_dev); //根据spi_device中的配置参数, 设置SPI控制器的硬件寄存器, 确保二者之间的通信参数匹配

    /********************************
     *          regmap初始化
     ********************************/
    pspi_oled_dev->pregmap = devm_regmap_init(&pspi_dev->dev, NULL, pspi_dev, &oled_regmap_config);

    /*打印结果*/
    printk("max_speed_hz = %d\n", pspi_oled_dev->pspi_dev->max_speed_hz);
	printk("chip_select = %d\n", (int)pspi_oled_dev->pspi_dev->chip_select);
	printk("bits_per_word = %d\n", (int)pspi_oled_dev->pspi_dev->bits_per_word);    //每个字的位数
	printk("mode = %02X\n", pspi_oled_dev->pspi_dev->mode);                         //模式标志位
	printk("cs_gpio = %02X\n", pspi_oled_dev->pspi_dev->cs_gpio);

    /*设置驱动私有数据*/
    spi_set_drvdata(pspi_dev, pspi_oled_dev);

    return 0;
add_err:
    unregister_chrdev_region(pspi_oled_dev->devno, 1);

    return -1;
}

static int spi_oled_remove(struct spi_device *pdev)
{
    /*需要释放字符资源*/
    if (!IS_ERR(pspi_oled_dev))
    {
        gpio_free(pspi_oled_dev->oled_dc_ctrl_pinno);
        /*设备管理*/
        device_destroy(pspi_oled_class, pspi_oled_dev->devno);
        class_destroy(pspi_oled_class);
        /*字符设备*/
        cdev_del(&pspi_oled_dev->cdev);					  
	    unregister_chrdev_region(pspi_oled_dev->devno, SPI_OLED_NUM);

        /*清空设备句柄*/
        pspi_oled_dev->pspi_dev = NULL;
        pspi_oled_dev->pregmap = NULL;
    }
    return 0;
}

/*****************************************************
 *                  模块初始化和退出
 ******************************************************/

static int __init spi_oled_init(void)
{
    int ret = 0;

    ret = spi_register_driver(&spi_oled_driver);

    return ret;
}
module_init(spi_oled_init);

static void __exit spi_oled_exit(void)
{
    spi_unregister_driver(&spi_oled_driver);
}
module_exit(spi_oled_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

