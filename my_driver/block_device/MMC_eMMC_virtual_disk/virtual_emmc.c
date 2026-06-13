
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/mmc/host.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sd.h>

#include <linux/mmc/mmc.h>

#include <linux/vmalloc.h>
#include <linux/slab.h>

#include <linux/workqueue.h>
#include <linux/platform_device.h>


/*****************************************************
 * 
 *                  模块简介
 * xxxxxx(模块功能)
 * xxxxxx(基本代码逻辑)
 *
 ******************************************************/

/*****************************************************
 *                  宏定义
 ******************************************************/
#define VIRTUAL_EMMC_SIZE_MB    512
#define VIRTUAL_EMMC_SECTOR_SIZE 512
#define VIRTUAL_EMMC_BLOCK_SIZE  512
#define VIRTUAL_EMMC_MAX_SEGS    128


/*****************************************************
 *                  类型定义
 ******************************************************/
struct virtual_emmc_host {

    struct mmc_host *mmc;
    struct mmc_request *mrq;
    struct mmc_command *cmd;
    struct mmc_data *data;

    void *ram_storage;
    unsigned long storage_size;

    spinlock_t lock;
    struct work_struct cmd_work;

    /*eMMC状态*/
    u32 ocr;
    u32 cid[4];
    u32 csd[4];
    u8 ext_csd[512];
    
    u32 rca;
    bool is_locked;

    struct platform_device *pdev;
};
/*****************************************************
 *                  函数声明
 ******************************************************/
// MMC请求处理
static void virtual_emmc_request(struct mmc_host *mmc, struct mmc_request *mrq);
// 设置IOS
static void virtual_emmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios);
// 获取卡检测状态
static int virtual_emmc_get_cd(struct mmc_host *mmc);
static int virtual_emmc_get_ro(struct mmc_host *mmc);

static int virtual_emmc_probe(struct platform_device *pdev);
static int virtual_emmc_remove(struct platform_device *pdev);

/*****************************************************
 *                  全局变量
 ******************************************************/
static struct platform_device_id virtual_emmc_ids[] = {
    {.name = "virtual_emmc"},
    {}
};
static struct platform_device virtual_emmc_device = {
    .name = "virtual_emmc",
    .driver_override = "virtual_emmc",
    .id_entry = virtual_emmc_ids,
    .num_resources = 0
};
static struct platform_driver virtual_emmc_driver = {
    .probe = virtual_emmc_probe,
    .remove = virtual_emmc_remove,
    .id_table = virtual_emmc_ids,
    .driver = {
        .name = "virtual_emmc",
    }
};

// MMC主机操作
static const struct mmc_host_ops virtual_emmc_ops = {
    .request = virtual_emmc_request,
    .set_ios = virtual_emmc_set_ios,
    .get_cd = virtual_emmc_get_cd,
    .get_ro = virtual_emmc_get_ro
};

/*****************************************************
 *                  辅助函数
 ******************************************************/
// 虚拟eMMC设备寄存器初始化
static void virtual_emmc_init_registers(struct virtual_emmc_host *host)
{
    u32 sectors = host->storage_size / VIRTUAL_EMMC_SECTOR_SIZE;
    // OCR寄存器
    host->ocr = MMC_VDD_165_195 | MMC_VDD_32_33 | MMC_VDD_33_34;
    
    // CID寄存器
    memset(host->cid, 0, sizeof(host->cid));
    host->cid[0] = 0x15010056;  // Manufacturer ID: 0x15
    host->cid[1] = 0x49525455;  // "VIRT"
    host->cid[2] = 0x414C454D;  // "ALEM"
    host->cid[3] = 0x4D430001;  // "MC" + serial
    
    // CSD寄存器 - 高容量eMMC (CSD_STRUCTURE=2)
    memset(host->csd, 0, sizeof(host->csd));
    host->csd[0] = 0xD00F0032;  // CSD_STRUCTURE=2, SPEC_VERS=4, TAAC
    host->csd[1] = 0x5B590000;  // NSAC, TRAN_SPEED
    host->csd[2] = 0x00007F7F;  // C_SIZE部分
    host->csd[3] = 0x0A404001;  // 其他参数
    
    // EXT_CSD寄存器
    memset(host->ext_csd, 0, sizeof(host->ext_csd));
    host->ext_csd[EXT_CSD_REV] = 5;  // eMMC 5.0
    host->ext_csd[EXT_CSD_SEC_FEATURE_SUPPORT] = 0x15;
    host->ext_csd[EXT_CSD_CARD_TYPE] = 0x07;  // HS200, HS400
    //host->ext_csd[EXT_CSD_DEVICE_VERSION] = 0x50;
    
    // 设置容量 (扇区数)
    host->ext_csd[EXT_CSD_SEC_CNT + 0] = sectors & 0xFF;
    host->ext_csd[EXT_CSD_SEC_CNT + 1] = (sectors >> 8) & 0xFF;
    host->ext_csd[EXT_CSD_SEC_CNT + 2] = (sectors >> 16) & 0xFF;
    host->ext_csd[EXT_CSD_SEC_CNT + 3] = (sectors >> 24) & 0xFF;
    
    host->rca = 0x0001;
    host->is_locked = false;
}

// 处理MMC命令
static void virtual_emmc_process_command(struct virtual_emmc_host *host)
{
    struct mmc_command *cmd = host->cmd;
    struct mmc_data *data = host->data;
    
    cmd->error = 0;
    
    switch (cmd->opcode) {
    case MMC_GO_IDLE_STATE:  // CMD0
        host->rca = 0;
        break;
        
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
    case MMC_SLEEP_AWAKE: // CMD5, 这是sdio
        cmd->error = -ETIMEDOUT;
        break;
    case 41:
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
        if (data && data->sg) {
            u32 sector = cmd->arg;
            unsigned long offset = (unsigned long)sector * VIRTUAL_EMMC_SECTOR_SIZE;
            size_t total_size = data->blocks * VIRTUAL_EMMC_SECTOR_SIZE;
            
            if (offset + total_size <= host->storage_size) {
                sg_copy_to_buffer(data->sg, data->sg_len,
                                host->ram_storage + offset, total_size);
                data->bytes_xfered = total_size;
            } else {
                cmd->error = -EINVAL;
                dev_err(mmc_dev(host->mmc), "Read out of bounds\n");
            }
        }
        break;
        
    case MMC_WRITE_BLOCK:         // CMD24
    case MMC_WRITE_MULTIPLE_BLOCK: // CMD25
        if (data && data->sg) {
            u32 sector = cmd->arg;
            unsigned long offset = (unsigned long)sector * VIRTUAL_EMMC_SECTOR_SIZE;
            size_t total_size = data->blocks * VIRTUAL_EMMC_SECTOR_SIZE;
            
            if (offset + total_size <= host->storage_size) {
                sg_copy_from_buffer(data->sg, data->sg_len,
                                  host->ram_storage + offset, total_size);
                data->bytes_xfered = total_size;
            } else {
                cmd->error = -EINVAL;
                dev_err(mmc_dev(host->mmc), "Write out of bounds\n");
            }
        }
        break;
        
    case MMC_SWITCH:             // CMD6
        cmd->resp[0] = 0x00000900;
        break;
        
    case MMC_SEND_EXT_CSD:      // CMD8
        if (data && data->sg) {
            sg_copy_to_buffer(data->sg, data->sg_len,
                            host->ext_csd, sizeof(host->ext_csd));
            data->bytes_xfered = sizeof(host->ext_csd);
        }
        break;
        
    default:
        if (cmd->opcode == 5 || cmd->opcode == 8 || cmd->opcode == 55 || cmd->opcode == 52) 
        {
            // 故意不设置cmd->error，让核心层超时，或者设置成 -ETIMEDOUT
            // 注意：不设置error，mmc_request_done后核心层会认为超时
            cmd->error = -ETIMEDOUT; 
        } 
        else
        {
            cmd->error = -EINVAL;
            dev_err(mmc_dev(host->mmc), "Unsupported command: %d\n", cmd->opcode);
        }
        break;
    }
}

// 命令处理工作队列
static void virtual_emmc_cmd_work(struct work_struct *work)
{
    struct virtual_emmc_host *host = container_of(work, struct virtual_emmc_host, cmd_work);
    unsigned long flags;
    
    spin_lock_irqsave(&host->lock, flags);
    
    if (host->mrq) {
        host->cmd = host->mrq->cmd;
        host->data = host->mrq->data;
        
        virtual_emmc_process_command(host);
        
        mmc_request_done(host->mmc, host->mrq);
        host->mrq = NULL;
    }
    
    spin_unlock_irqrestore(&host->lock, flags);
}

/*****************************************************
 *                  设备操作
 ******************************************************/
// MMC请求处理
static void virtual_emmc_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
    struct virtual_emmc_host *host = mmc_priv(mmc);
    unsigned long flags;
    
    spin_lock_irqsave(&host->lock, flags);
    
    if (host->mrq) {
        spin_unlock_irqrestore(&host->lock, flags);
        mrq->cmd->error = -EBUSY;
        mmc_request_done(mmc, mrq);
        return;
    }
    
    host->mrq = mrq;
    spin_unlock_irqrestore(&host->lock, flags);
    
    schedule_work(&host->cmd_work);
}

// 设置IOS
static void virtual_emmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
    dev_err(mmc_dev(mmc), "Set IOS: clock=%d, bus_width=%d, timing=%d\n",
            ios->clock, ios->bus_width, ios->timing);
}

// 获取卡检测状态
static int virtual_emmc_get_cd(struct mmc_host *mmc)
{
    return 1;  // 卡始终存在
}

static int virtual_emmc_get_ro(struct mmc_host *mmc)
{
    return 0;  // 可读写
}

/*****************************************************
 *                  驱动操作
 ******************************************************/
static int virtual_emmc_probe(struct platform_device *pdev)
{
    struct virtual_emmc_host *host;
    struct mmc_host *mmc;
    int ret;
    
    dev_info(&pdev->dev, "Virtual eMMC Driver Probing\n");
    
    // 分配MMC主机，传递有效的device指针
    mmc = mmc_alloc_host(sizeof(struct virtual_emmc_host), &pdev->dev);
    if (!mmc) {
        dev_err(&pdev->dev, "Failed to allocate MMC host\n");
        return -ENOMEM;
    }
    
    host = mmc_priv(mmc);
    host->mmc = mmc;
    host->pdev = pdev;
    platform_set_drvdata(pdev, host);
    
    // 分配RAM存储
    host->storage_size = VIRTUAL_EMMC_SIZE_MB * 1024 * 1024;
    host->ram_storage = vzalloc(host->storage_size);
    if (!host->ram_storage) {
        dev_err(&pdev->dev, "Failed to allocate RAM storage\n");
        ret = -ENOMEM;
        goto err_free_mmc;
    }
    
    // 初始化eMMC寄存器
    virtual_emmc_init_registers(host);
    
    // 初始化锁和工作队列
    spin_lock_init(&host->lock);
    INIT_WORK(&host->cmd_work, virtual_emmc_cmd_work);
    
    // 设置MMC主机参数
    mmc->ops = &virtual_emmc_ops;
    mmc->f_min = 400000;      // 400kHz
    mmc->f_max = 200000000;   // 200MHz
    mmc->ocr_avail = MMC_VDD_165_195 | MMC_VDD_32_33 | MMC_VDD_33_34;
    mmc->caps = MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA |
                MMC_CAP_MMC_HIGHSPEED | MMC_CAP_1_8V_DDR |
                MMC_CAP_CMD23;
    mmc->caps2 = MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_HS400_1_8V;
    mmc->max_segs = VIRTUAL_EMMC_MAX_SEGS;
    mmc->max_seg_size = 512 * 1024;
    mmc->max_blk_size = 512;
    mmc->max_blk_count = 256;
    mmc->max_req_size = 512 * 1024;
    
    // 注册MMC主机
    ret = mmc_add_host(mmc);
    if (ret) {
        dev_err(&pdev->dev, "Failed to add MMC host\n");
        goto err_free_ram;
    }
    
    dev_info(&pdev->dev, "Virtual eMMC: %dMB RAM disk on MMC bus\n", 
             VIRTUAL_EMMC_SIZE_MB);
    
    return 0;
    
err_free_ram:
    vfree(host->ram_storage);
err_free_mmc:
    mmc_free_host(mmc);
    return ret;
}

static int virtual_emmc_remove(struct platform_device *pdev)
{
    printk("remove come in\r\n");

    struct virtual_emmc_host *host = platform_get_drvdata(pdev);
    
    if (host)
    {
        cancel_work_sync(&host->cmd_work);
        mmc_remove_host(host->mmc);
        vfree(host->ram_storage);
        mmc_free_host(host->mmc);
    }
    
    dev_info(&pdev->dev, "Virtual eMMC Driver removed\n");
    return 0;
}
/*****************************************************
 *                  模块初始化和退出
 ******************************************************/
static int __init virtual_emmc_init(void)
{
    int ret = 0;
    printk("virtual_emmc_init\r\n");
    /*需要把device加载到内核中*/
    platform_device_register(&virtual_emmc_device);
    /*需要把driver加载到内核中*/
    ret = platform_driver_register(&virtual_emmc_driver);
    if (ret)
    {
        printk("virtual_emmc_init driver register error\r\n");
    }
    return ret;
}
module_init(virtual_emmc_init);

static void __exit virtual_emmc_exit(void)
{
    printk("virtual_emmc_exit\r\n");
    /*卸载device*/
    platform_device_unregister(&virtual_emmc_device);
    /*卸载驱动*/
    platform_driver_unregister(&virtual_emmc_driver);
}
module_exit(virtual_emmc_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("DYWorker001");
