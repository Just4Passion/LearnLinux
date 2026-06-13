

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>

#include <linux/slab.h>
#include <linux/uaccess.h>

#include <linux/io.h>
#include <linux/gpio.h>

#include <linux/device.h>
#include <linux/i2c.h>

#include "i2c_mpu6050.h"

/*****************************************************
 * 
 *                  模块简介
 * i2c_adapter: 芯片提供商实现
 * i2c_alogrithm: 芯片提供商实现
 * i2c_driver:
 * i2c_client:
 * i2c_msg: 
 *
 ******************************************************/

/*****************************************************
 *                  宏定义
 ******************************************************/
#define I2C_MPU6050_NAME "i2c1_mpu6050"
#define I2C_MPU6050_NUM (1)

/*****************************************************
 *                  类型定义
 ******************************************************/
struct i2c_mpu6050_device {
    dev_t devno;
    struct cdev cdev;
    struct device *pdev;

    struct i2c_client *pi2c_dev;

};

/*****************************************************
 *                  函数声明
 ******************************************************/
static int mpu6050_open(struct inode *inode, struct file *filep);
static int mpu6050_release(struct inode *inode, struct file *filep);
static ssize_t mpu6050_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos);

/*驱动操作函数*/
static int i2c_mpu6050_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int i2c_mpu6050_remove(struct i2c_client *client);

/*****************************************************
 *                  全局变量
 ******************************************************/
/*字符设备操作函数集*/
struct file_operations i2c_mpu6050_fops = {
    .owner = THIS_MODULE,
    .open = mpu6050_open,
    .release = mpu6050_release,
    .write = NULL,
    .read = mpu6050_read,
    .unlocked_ioctl = NULL,
};

static int i2c_mpu6060_major = 0;
/*i2c设备*/
struct i2c_mpu6050_device *pi2c_mpu_dev = NULL;
struct class *i2c_mpu_class;


/*i2c_device_id*/
static const struct i2c_device_id i2c_match_ids[] = {
	{"fire,i2c_mpu6050", 0},
	{}
};

/*of_device_id, 用于compatible匹配*/
struct of_device_id mpu6050_match_ids[] = {
    {.compatible = "fire,i2c_mpu6050"},
    {}
};

/*I2C driver*/
struct i2c_driver i2c_mpu6050_driver = {
    .probe = i2c_mpu6050_probe,
    .remove = i2c_mpu6050_remove,
    .command = NULL,
    .id_table = i2c_match_ids,
    .driver = 
    {
        .owner = THIS_MODULE,
        .name = "i2c_mpu6050_driver",
        .of_match_table = mpu6050_match_ids
    }
};

/*****************************************************
 *                  辅助函数
 ******************************************************/
/**
 * @brief 向mpu6050写入数据
 * @param mpu6050_client: i2c设备
 * @param address: 数据地址
 * @param data: 数据
 * @note 
 */
static int i2c_write_mpu6050(struct i2c_client *mpu6050_client, u8 address, u8 data)
{
    int ret = 0;

    u8 write_data[2];

    struct i2c_msg send_msg;

    write_data[0] = address;
	write_data[1] = data;

    /*发送消息打包*/
    send_msg.addr = mpu6050_client->addr;
    send_msg.flags = 0;     //写入
    send_msg.buf = write_data;
    send_msg.len = 2;

    /*发送*/
    ret = i2c_transfer(mpu6050_client->adapter, &send_msg, 1);
    if (ret != 1)
    {
        dev_err(&mpu6050_client->dev, "write i2c transfer faield\r\n");
        return -1;
    }

    return 0;
}

/**
 * @brief 从mpu6050读取数据
 * @param mpu6050_client: i2c设备
 * @param address: 数据地址
 * @param data: 数据
 * @note 
 */
static int i2c_read_mpu6050(struct i2c_client *mpu6050_client, u8 address, void *data, u32 length)
{
    int ret = 0;

    u8 address_data = address;

    struct i2c_msg mpu6050_msg[2];
    /*先写入要读取的数据的地址*/
    mpu6050_msg[0].addr = mpu6050_client->addr;
    mpu6050_msg[0].flags = 0;                     //写入
    mpu6050_msg[0].buf = &address_data;
    mpu6050_msg[0].len = 1;

    /*然后读取数据*/
    mpu6050_msg[1].addr = mpu6050_client->addr;
    mpu6050_msg[1].flags = I2C_M_RD;             //读取
    mpu6050_msg[1].buf = data;
    mpu6050_msg[1].len = length;

    /*发送*/
    ret = i2c_transfer(mpu6050_client->adapter, mpu6050_msg, 1);
    if (ret != 2)
    {
        dev_err(&mpu6050_client->dev, "read i2c transfer faield\r\n");
        return -1;
    }
    return 0;
}

/**
 * @brief mpu6050初始化
 * @note 
 */
static int mpu6050_init(struct i2c_client *mpu6050_client)
{
    int ret = 0;
    ret += i2c_write_mpu6050(mpu6050_client, PWR_MGMT_1, 0x00);
    ret += i2c_write_mpu6050(mpu6050_client, SMPLRT_DIV, 0x07);
	ret += i2c_write_mpu6050(mpu6050_client, CONFIG, 0x06);
	ret += i2c_write_mpu6050(mpu6050_client, ACCEL_CONFIG, 0x01);

    if (ret < 0)
    {
        dev_err(&mpu6050_client->dev, "mpu6050_init error \r\n");
		return -1;
    }
    return 0;
}


/*****************************************************
 *                  字符设备操作
 ******************************************************/
static int mpu6050_open(struct inode *inode, struct file *filep)
{
    int ret = 0;
    struct i2c_mpu6050_device *pmpu_dev = container_of(inode->i_cdev, struct i2c_mpu6050_device, cdev);

    ret = mpu6050_init(pmpu_dev->pi2c_dev);
    if (ret < 0)
    {
        return -1;
    }

    filep->private_data = pmpu_dev;

    return 0;
}


static int mpu6050_release(struct inode *inode, struct file *filep)
{
    //int ret = 0;
    //struct i2c_mpu6050_device *pmpu_dev = filep->private_data;

    /*向mpu6050发送命令，使mpu6050进入关机状态*/
    return 0;
}


static ssize_t mpu6050_read(struct file *filep, char __user *buf, size_t count, loff_t *ppos)
{
    char data_H = 0;
    char data_L = 0;
    int ret = 0;

    /*六轴传感器, 记录数据*/
    short mpu6050_result[6] = {0};

    struct i2c_mpu6050_device *pmpu_dev = filep->private_data;

    ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, ACCEL_XOUT_H, &data_H, 1);
	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, ACCEL_XOUT_L, &data_L, 1);
	mpu6050_result[0] = data_H << 8;
	mpu6050_result[0] += data_L;

	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, ACCEL_YOUT_H, &data_H, 1);
	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, ACCEL_YOUT_L, &data_L, 1);
	mpu6050_result[1] = data_H << 8;
    mpu6050_result[1] += data_L;

	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, ACCEL_ZOUT_H, &data_H, 1);
	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, ACCEL_ZOUT_L, &data_L, 1);
	mpu6050_result[2] = data_H << 8;
	mpu6050_result[2] += data_L;

	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, GYRO_XOUT_H, &data_H, 1);
	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, GYRO_XOUT_L, &data_L, 1);
	mpu6050_result[3] = data_H << 8;
	mpu6050_result[3] += data_L;

	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, GYRO_YOUT_H, &data_H, 1);
	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, GYRO_YOUT_L, &data_L, 1);
	mpu6050_result[4] = data_H << 8;
	mpu6050_result[4] += data_L;

	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, GYRO_ZOUT_H, &data_H, 1);
	ret = i2c_read_mpu6050(pmpu_dev->pi2c_dev, GYRO_ZOUT_L, &data_L, 1);
	mpu6050_result[5] = data_H << 8;
	mpu6050_result[5] += data_L;

    return 0;
}


/*****************************************************
 *                  平台驱动操作
 ******************************************************/
static int i2c_mpu6050_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;

    /*分配资源*/
    pi2c_mpu_dev = devm_kzalloc(&client->dev, sizeof(struct i2c_mpu6050_device), GFP_KERNEL);
    if (IS_ERR(pi2c_mpu_dev))
    {
        ret = PTR_ERR(pi2c_mpu_dev);
        dev_err(&client->dev, "failed to malloc i2c_mpu6050_device\r\n");
        return ret;
    }
    /*i2c_client赋值*/
    pi2c_mpu_dev->pi2c_dev = client;
    /**************************************
     * 
     *          字符设备初始化
     * 
     ***************************************/
    ret = alloc_chrdev_region(&i2c_mpu6060_major, 0, I2C_MPU6050_NUM, I2C_MPU6050_NAME);
    if (ret < 0)
    {
        dev_err(&client->dev, "failed to alloc mpu6050 devno\r\n");
        return ret;
    }

    cdev_init(&pi2c_mpu_dev->cdev, &i2c_mpu6050_fops);
    pi2c_mpu_dev->cdev.owner = THIS_MODULE;

    ret = cdev_add(&pi2c_mpu_dev->cdev, i2c_mpu6060_major, I2C_MPU6050_NUM);
    if (ret < 0)
    {
        dev_err(&client->dev, "failed to add cdev\r\n");
        goto add_err;
    }

    /*创建类*/
    i2c_mpu_class = class_create(THIS_MODULE, I2C_MPU6050_NAME);
    /*创建设备*/
    pi2c_mpu_dev->pdev = device_create(i2c_mpu_class, NULL, i2c_mpu6060_major, NULL, I2C_MPU6050_NAME);

    /*设置驱动参数*/
    i2c_set_clientdata(client, pi2c_mpu_dev);

    return 0;
add_err:
    unregister_chrdev_region(i2c_mpu6060_major, I2C_MPU6050_NUM);

    return -1;
}

static int i2c_mpu6050_remove(struct i2c_client *client)
{
    if (pi2c_mpu_dev != NULL)
    {
        /*删除设备*/
        device_destroy(i2c_mpu_class, i2c_mpu6060_major);
        class_destroy(i2c_mpu_class);
        /*删除字符设备*/
        cdev_del(&pi2c_mpu_dev->cdev);
        unregister_chrdev_region(i2c_mpu6060_major, I2C_MPU6050_NUM);
    }

    return 0;
}

/*****************************************************
 *                  模块初始化和退出
 ******************************************************/
static int __init i2c_mpu6050_init(void)
{
    int ret;
	pr_info("i2c_mpu6050_init\r\n");
	ret = i2c_add_driver(&i2c_mpu6050_driver);
    return ret;
}
module_init(i2c_mpu6050_init);

static void __exit i2c_mpu6050_exit(void)
{
    pr_info("i2c_mpu6050_exit\r\n");
	i2c_del_driver(&i2c_mpu6050_driver);
}
module_exit(i2c_mpu6050_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");


