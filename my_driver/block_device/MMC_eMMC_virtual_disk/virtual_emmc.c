
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>


#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>

#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>


#include <linux/platform_device.h>

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/workqueue.h>


/*****************************************************
 * 
 *                      模块简介
 * 源码基本逻辑
 *      1. 内存虚拟成存储卡
 *      2. 需要编写自己的MMC Host逻辑, 去控制这张内存卡
 * 
 * 编码逻辑
 *      1. 驱动probe和remove
 *          - mmc_alloc_host, mmc_add_host
 *      2. 设备节点
 *      3. 设备操作函数: struct mmc_host_ops
 *      4. 命令处理函数
 * 
 * 基本概念
 *      1. Page: Flash内部的物理读写单位
 *      2. Sector: 存储设备可寻址和传输的最小单位
 *      3. Block: 内核管理内存和文件系统的最小逻辑单位
 *          - 文件系统在格式化时创建的一个逻辑容器. 由连续的2^n个sector组成
 * 
 * SoC内嵌的MMC/SD/SDIO控制器是SDHCI(Secure Digital Host Controller Interface)
 *
 * 
 * 驱动接入工作
 *      核心: 让内核知道如何与你的具体的硬件控制器通信. drivers/mmc/host
 *      
 * 场景1: 使用现成的SDHCI兼容控制器
 *      1. 查看SoC数据手册, 确认MMC控制器是否是SDHCI兼容的
 *      2. 在drivers/mmc/host/sdhci_myboard.c, 实现你的SDHCI驱动
 *          - sdhci_pltfm_init
 *          - sdhci_add_host
 *      3. 编写设备树节点: 添加mmc节点配置
 * 
 * 场景2: 使用SoC源码中已经实现的控制器
 *      1. 有些 SoC 的 MMC 控制器不是标准的 SDHCI，但已经有对应的驱动框架
 *          Allwinner (sunxi) 平台：sunxi-mmc.c
 *          TI Davinci 平台：davinci_mmc.c
 *          NVIDIA Tegra 平台：sdhci-tegra.c
 *          Renesas R-Car 平台：renesas_sdhi.
 *      2. 查找是否已经存在驱动
 *      3. 编写设备树节点
 *          - 控制器的物理地址和中断
 *          - 时钟配置
 *          - 总线宽度
 *          - 卡检测引脚
 *          - 电源管理
 * 
 * 场景3: 从零开始编写全新的控制器驱动
 *      1. 理解硬件数据手册
 *          - 命令寄存器：如何发送命令、接收响应
 *          - 数据寄存器：如何读写数据
 *          - 状态寄存器：中断状态、错误状态
 *          - DMA 引擎：如果有的话
 *          - 时钟控制：如何配置 MMC 时钟频率
 *          - 电源控制：如何控制卡槽电源
 *          - 卡检测机制：轮询还是中断
 *      2. 实现核心数据结构
 *          - struct my_mmc_host
 *              - struct mmc_host *mmc;
 *              - void __iomem *base;        // 寄存器基址
 *              - int irq;                   // 中断号
 *              - struct clk *clk;           // 时钟
 *              - struct mmc_request *mrq;   // 当前正在处理的请求
 *              - struct mmc_command *cmd;   // 当前命令
 *              - struct mmc_data *data;     // 当前数据传输
 *              - spinlock_t lock;           // 保护并发访问
 *              - struct completion cmd_done; // 命令完成通知
 *              - struct completion data_done; // 数据传输完成通知
 *          - struct mmc_host_ops my_mmc_ops
 *              - .request = my_mmc_request,           // 核心：处理 MMC 请求
 *              - .set_ios = my_mmc_set_ios,           // 配置时钟、总线宽度、电压
 *              - .get_ro = my_mmc_get_ro,             // 读卡器写保护状态
 *              - .get_cd = my_mmc_get_cd,             // 读卡器卡检测状态
 *              - .enable_sdio_irq = my_mmc_enable_sdio_irq, // SDIO 中断
 *              - .start_signal_voltage_switch = my_mmc_voltage_switch, // 电压切换
 *              - .execute_tuning = my_mmc_execute_tuning, // HS200/HS400 调谐
 *              - .card_busy = my_mmc_card_busy,       // 检测卡是否忙
 *      3. 处理中断, DMA, 电源管理
 * 
 * 
 ******************************************************/

/*****************************************************
 *                  宏定义
 ******************************************************/
#define VIRTUAL_EMMC_SIZE_MB        512     // 虚拟eMMC, 大小512MB
#define VIRTUAL_EMMC_SECTOR_SIZE    512     // 单个sector大小
#define VIRTUAL_EMMC_BLOCK_SIZE     512     // 单个块的大小
#define VIRTUAL_EMMC_MAX_SEGS       128     // 


/*****************************************************
 *                  类型定义
 ******************************************************/
struct virtual_emmc_host {
    struct mmc_host *mmc;
    struct mmc_request *mrq;        // request
    struct mmc_command *cmd;
    struct mmc_data *data;


    void *ram_storage;              // RAM存储区域
    unsigned long storage_size;     // 存储大小

    spinlock_t lock;
    struct timer_list cmd_timer;    // 定时器
    struct work_struct cmd_work;    // 工作队列

    /*eMMC状态*/
    u32 ocr;
    u32 cid[4];             //
    u32 csd[4];             //
    u8 ext_csd[512];        //扩展卡片特性数据
    u32 rca;
    bool is_locked;
};



/*****************************************************
 *                  函数声明
 ******************************************************/
/*操作函数*/
static void virtual_emmc_request(struct mmc_host *mmc, struct mmc_request *mrq);
static void virtual_emmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios);
static int virtual_emmc_get_cd(struct mmc_host *mmc);

/*驱动操作函数*/
static int virtual_emmc_probe(struct platform_device *pdev);
static int virtual_emmc_remove(struct platform_device *pdev);

/*****************************************************
 *                  全局变量
 ******************************************************/
static const struct mmc_host_ops virtual_emmc_ops = {
    .request = virtual_emmc_request,
    .set_ios = virtual_emmc_set_ios,
    .get_cd = virtual_emmc_get_cd
};

static struct of_device_id virtual_emmc_ids[] = {
    {.compatible = "virtual,emmc"},
    {}
};

/*可以在设备树中增加一个节点, 这样就可以进入probe执行*/
static struct platform_driver virtual_emmc_driver = {
    .probe = virtual_emmc_probe,
    .remove = virtual_emmc_remove,
    .driver = {
        .name = "virtual_mmc",
        .of_match_table = virtual_emmc_ids
    }
};

/*****************************************************
 *                  辅助函数
 ******************************************************/
/**
 * @brief 初始化emmc寄存器
 * @param host emmc主控制器
 * @note 
 */
static void virtual_emmc_init_register(struct virtual_emmc_host *host)
{
    unsigned long sectors = host->storage_size / VIRTUAL_EMMC_SECTOR_SIZE;

    /*OCR寄存器*/
    host->ocr = 0x00FF8080;

    /*CID寄存器: 卡的唯一 ID、制造商、产品名等信息*/
    host->cid[0] = 0x15010056;  // Manufacturer ID: 0x15, OEM/Application ID: 0x0100
    host->cid[1] = 0x49525455;  // Product name: "VIRT"
    host->cid[2] = 0x414C454D;  // Product name: "ALEM"
    host->cid[3] = 0x4D430001;  // Product name: "MC", serial: 0x0001

    /*CSD寄存器 (支持高容量): 卡的容量、最大时钟频率、最大块大小、读写电流*/
    memset(host->csd, 0, sizeof(host->csd));
    host->csd[0] = 0x400E0032;  // CSD_STRUCTURE=2, SPEC_VERS=4
    host->csd[1] = 0x5B590000;  // TAAC, NSAC, TRAN_SPEED
    host->csd[2] = 0x00007F7F;  // C_SIZE部分
    host->csd[3] = 0x0A404001;  // 其他参数

    /*EXT_CSD寄存器*/
    memset(host->ext_csd, 0, sizeof(host->ext_csd));
    host->ext_csd[EXT_CSD_REV] = 5;  // eMMC 5.0
    host->ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT] = 0x15;
    host->ext_csd[EXT_CSD_CARD_TYPE] = 0x07;  // HS200, HS400

    /*设置容量*/
    host->ext_csd[EXT_CSD_SEC_CNT + 0] = sectors & 0xFF;
    host->ext_csd[EXT_CSD_SEC_CNT + 1] = (sectors >> 8) & 0xFF;
    host->ext_csd[EXT_CSD_SEC_CNT + 2] = (sectors >> 16) & 0xFF;
    host->ext_csd[EXT_CSD_SEC_CNT + 3] = (sectors >> 24) & 0xFF;
    
    /*RCA (Relative Card Address): 在总线上的动态地址，由主机在初始化时分配*/
    host->rca = 0x0001;

    /*锁状态*/
    host->is_locked = false;
}

/**
 * @brief 处理MMC命令
 * @param host emmc主控制器
 * @note 
 */
static void virtual_emmc_process_command(struct virtual_emmc_host *host)
{
    struct mmc_command *cmd = host->cmd;

    cmd->error = 0;

    switch (cmd->opcode)
    {
        case MMC_GO_IDLE_STATE:  // CMD0
        host->rca = 0;
        break;
        
        /*eMMC需要处理改命令, 返回ocr*/
        case MMC_SEND_OP_COND:   // CMD1
            cmd->resp[0] = host->ocr;
            break;
            
        case MMC_ALL_SEND_CID:   // CMD2
            memcpy(cmd->resp, host->cid, sizeof(host->cid));
            break;
            
        case MMC_SET_RELATIVE_ADDR: // CMD3
            host->rca = cmd->arg >> 16;
            cmd->resp[0] = (host->rca << 16) | 0x0001;
            break;

        /*SDIO总线需要处理该命令, 用于复位卡片*/
        case MMC_SLEEP_AWAKE:       // CMD5, 这里虚拟eMMC, 不做处理
            cmd->error = -ETIMEDOUT;
            break;

        /*SD需要处理该命令, APP命令*/
        case 41:                    // CMD41, 这里虚拟eMMC, 不做处理
            cmd->error = -ETIMEDOUT;
            break;
            
        case MMC_SEND_CSD:       // CMD9
            memcpy(cmd->resp, host->csd, sizeof(host->csd));
            break;
            
        case MMC_SEND_CID:       // CMD10
            memcpy(cmd->resp, host->cid, sizeof(host->cid));
            break;
            
        case MMC_STOP_TRANSMISSION: // CMD12
            break;
            
        case MMC_SEND_STATUS:    // CMD13
            cmd->resp[0] = 0x00000900;  // Ready state
            break;
        case MMC_READ_SINGLE_BLOCK:   // CMD17
        case MMC_READ_MULTIPLE_BLOCK: // CMD18
            if (host->data && host->data->sg) {
                u32 sector = cmd->arg;
                unsigned long offset = (unsigned long)sector * VIRTUAL_EMMC_SECTOR_SIZE;
                
                if (offset + host->data->blocks * VIRTUAL_EMMC_SECTOR_SIZE <= host->storage_size) 
                {
                    sg_copy_to_buffer(host->data->sg, host->data->sg_len,
                                    host->ram_storage + offset,
                                    host->data->blocks * VIRTUAL_EMMC_SECTOR_SIZE);
                    host->data->bytes_xfered = host->data->blocks * VIRTUAL_EMMC_SECTOR_SIZE;
                } 
                else
                {
                    cmd->error = -EINVAL;
                }
            }
            break;
        case MMC_WRITE_BLOCK:         // CMD24
        case MMC_WRITE_MULTIPLE_BLOCK: // CMD25
            if (host->data && host->data->sg) 
            {
                u32 sector = cmd->arg;
                unsigned long offset = (unsigned long)sector * VIRTUAL_EMMC_SECTOR_SIZE;
                
                if (offset + host->data->blocks * VIRTUAL_EMMC_SECTOR_SIZE <= host->storage_size) 
                {
                    sg_copy_from_buffer(host->data->sg, host->data->sg_len,
                                    host->ram_storage + offset,
                                    host->data->blocks * VIRTUAL_EMMC_SECTOR_SIZE);
                    host->data->bytes_xfered = host->data->blocks * VIRTUAL_EMMC_SECTOR_SIZE;
                } 
                else 
                {
                    cmd->error = -EINVAL;
                }
            }
            break;
        case MMC_SWITCH:             // CMD6
            cmd->resp[0] = 0x00000900;
            break;
        
        case MMC_SEND_EXT_CSD:      // CMD8
            if (host->data && host->data->sg) {
                sg_copy_to_buffer(host->data->sg, host->data->sg_len,
                                host->ext_csd, sizeof(host->ext_csd));
                host->data->bytes_xfered = sizeof(host->ext_csd);
            }
            break;
            
        default:
            cmd->error = -EINVAL;
            dev_err(mmc_dev(host->mmc), "Unsupported command: %d\n", cmd->opcode);
            break;
    }
}

/**
 * @brief 工作队列处理函数
 * @param host emmc主控制器
 * @note 
 */
static void virtual_emmc_cmd_work(struct work_struct *work)
{
    struct virtual_emmc_host *host = container_of(work, struct virtual_emmc_host, cmd_work);

    unsigned long flags;

    spin_lock_irqsave(&host->lock, flags);

    if (host->mrq)
    {
        host->cmd = host->mrq->cmd;
        host->data = host->mrq->data;

        /*处理命令*/
        virtual_emmc_process_command(host);

        /*完成请求*/
        host->mrq->cmd->error = host->cmd->error;
        if (host->mrq->data)
        {
             host->mrq->data->error = 0;
        }

        mmc_request_done(host->mmc, host->mrq);
        host->mrq = NULL;
    }

    spin_unlock_irqrestore(&host->lock, flags);
}

/*****************************************************
 *                  设备操作
 ******************************************************/
/**
 * @brief MMC请求处理
 * @param mmc 控制器
 * @param mrq 请求
 * @note 
 */
static void virtual_emmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
    struct virtual_emmc_host *host = mmc_priv(mmc);
    unsigned long flags;

    spin_lock_irqsave(&host->lock, flags);

    if (host->mrq)
    {
        spin_unlock_irqrestore(&host->lock, flags);
        mrq->cmd->error = -EBUSY;
        mmc_request_done(mmc, mrq);
        return;
    }

    host->mrq = mrq;
    spin_unlock_irqrestore(&host->lock, flags);

    schedule_work(&host->cmd_work);
}

/**
 * @brief 设置IOS
 * @param mmc 控制器
 * @param ios 
 * @note 
 */
static void virtual_emmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
    dev_dbg(mmc_dev(mmc), "Set IOS: clock=%d, bus_width=%d, timing=%d\n",
            ios->clock, ios->bus_width, ios->timing);
}

/**
 * @brief 获取卡状态
 * @param mmc 控制器
 * @note 
 */
static int virtual_emmc_get_cd(struct mmc_host *mmc)
{
    return 1; // 卡始终存在
}
/*****************************************************
 *                  驱动操作
 ******************************************************/
static int virtual_emmc_probe(struct platform_device *pdev)
{
    struct virtual_emmc_host *host;
    struct mmc_host *mmc;
    int ret = 0;

    dev_err(&pdev->dev, "virtual emmc driver initializing\r\n");

    /*********************************************
     * 分配了sizeof(struct mmc_host) + sizeof(struct virtual_emmc_host)的空间
     * struct mmc_host的最后一个成员是: unsigned long		private[0]
     */
    mmc = mmc_alloc_host(sizeof(struct virtual_emmc_host), &pdev->dev);
    if (!mmc)
    {
        dev_err(&pdev->dev, "mmc alloc host failed\r\n");
        return -ENOMEM;
    }

    /*返回private*/
    host = mmc_priv(mmc);
    host->mmc = mmc;

    /*分配RAM存储作为模拟的存储卡*/
    host->storage_size = VIRTUAL_EMMC_SIZE_MB * 1024 * 1024;
    host->ram_storage = vmalloc(host->storage_size);
    if (!host->ram_storage)
    {
        printk("Failed to allocate RAM storage\r\n");
        ret = -ENOMEM;
        goto err_free_mmc;
    }

    /*初始化RAM存储*/
    memset(host->ram_storage, 0, host->storage_size);

    /*初始化emmc寄存器*/
    virtual_emmc_init_register(host);


    /****************************************************
     * 
     *              初始化锁和工作队列
     * 
     */
    spin_lock_init(&host->lock);
    INIT_WORK(&host->cmd_work, virtual_emmc_cmd_work);


    /*设置MMC主机参数*/
    mmc->ops = &virtual_emmc_ops;
    mmc->f_min = 400000;    // 400kHz, MMC协议固定的初始化频率
    mmc->f_max = 200000000; // 200MHz, HS400模式的典型频率
    mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34; //电压范围
    mmc->caps = MMC_CAP_4_BIT_DATA |    // 4线数据模式
                MMC_CAP_8_BIT_DATA |    // 8线数据模式
                MMC_CAP_MMC_HIGHSPEED | // MMC高速模式    
                MMC_CAP_1_8V_DDR |      // 支持1.8V双倍速率模式
                MMC_CAP_CMD23;          // 支持CMD23(设置块数量)命令
    mmc->caps2 = MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_HS400_1_8V; //支持 HS200 模式（200MHz，单倍数据率）; 支持 HS400 模式（200MHz，双倍数据率）
    mmc->max_segs = VIRTUAL_EMMC_MAX_SEGS;      // 一个DMA请求能包含的最大内存段数量
    mmc->max_seg_size = 512 * 1024;             // 每个 DMA 内存段的最大字节数
    mmc->max_blk_size = 512;            // 主机控制器能处理的最大块（Block）大小（字节）
    mmc->max_blk_count = 256;           // 一个 MMC 命令能传输的最大块数量
    mmc->max_req_size = 512 * 1024;     // 一个 MMC 请求（可能包含多条命令）能传输的总数据量上限

    /*注册MMC主机*/
    ret = mmc_add_host(mmc);
    if (ret)
    {
        dev_err(&pdev->dev, "Failed to add MMC host\r\n");
        goto err_free_ram;
    }

    platform_set_drvdata(pdev, mmc);
    dev_err(&pdev->dev, "virtual emmc: %dMB RAM disk on MMC bus\r\n", VIRTUAL_EMMC_SIZE_MB);

    return 0;
err_free_ram:
    vfree(host->ram_storage);
err_free_mmc:
    mmc_free_host(mmc);
    return ret;
}

static int virtual_emmc_remove(struct platform_device *pdev)
{
    struct mmc_host *mmc = platform_get_drvdata(pdev);
    struct virtual_emmc_host *host = mmc_priv(mmc);

    printk("virtual emmc driver removed\r\n");
    if (mmc)
    {
        cancel_work_sync(&host->cmd_work);
        mmc_remove_host(mmc);
        vfree(host->ram_storage);
        mmc_free_host(mmc);
    }
    return 0;
}

/*****************************************************
 *                  模块初始化和退出
 ******************************************************/
#if 1
static int __init virtual_emmc_init(void)
{
    int ret = 0;
    ret = platform_driver_register(&virtual_emmc_driver);
    return ret;
}
module_init(virtual_emmc_init);

static void __exit virtual_emmc_exit(void)
{
    platform_driver_unregister(&virtual_emmc_driver);
}
module_exit(virtual_emmc_exit);
#endif

//module_platform_driver(virtual_emmc_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");

