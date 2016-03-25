/*
 * mmc-ali.c -MMC/SD/SDIO driver for ALi SoCs
 *
 * Copyright (C) 2014-2015 ALi Corporation - http://www.alitech.com
 *
 * Authors: David.Shih <david.shih@alitech.com>,
 *          Lucas.Lai  <lucas.lai@alitech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/blkdev.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>

#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/dmaengine.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <linux/mmc/sdio.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/mmc.h>

#include <asm/dma.h>
#include <asm/irq.h>

#include <linux/ali_interrupt.h>
#include <asm/mach-ali/m36_gpio.h>

#include "mmc-ali.h"

#define BUF_SIZE   (64*1024)//(16*1024)

#define WATCH_DOG_CMD_TIMEOUT	msecs_to_jiffies(300) 
#define WATCH_DOG_DATA_TIMEOUT	msecs_to_jiffies(2000) 

extern void ali_mmc_soc_reset(void);
extern void ali_mmc_soc_clock_gate(bool clock_gated);
extern void ali_mmc_pinmux_set(unsigned int mmc_group);
extern void ali_mmc_pinmux_restore(unsigned int mmc_group);

static int ali_mmc_get_cd(struct mmc_host *mmc);

static bool is_reg_0x48_writeable(struct ali_mmc_host *host)
{
	ali_mmc_writeb(host, 0x01, 0x48);

	if (ali_mmc_readb(host, 0x48) == 0x01) {
		ali_mmc_writeb(host, 0x00, 0x48);
		return true;
	} else
		return false;
}

static u32 ali_mmc_get_hw_rev(struct ali_mmc_host *host)
{
	if (is_reg_0x48_writeable(host))
		return ali_mmc_readl(host, REG_V19_HC_VERSION);
	else
		return (u32) ali_mmc_readb(host, REG_V18_HC_VERSION);
}

static void ali_mmc_reset(struct ali_mmc_host *host)
{
	/* IP Reset and recovery register setting */
	ali_mmc_soc_reset();
	host->ops->set_clock(host, host->clock);
	host->ops->set_bus_width(host, host->bus_width);
	host->ops->set_timing(host, host->timing);
}

static int ali_mmc_check_status(struct ali_mmc_host *host)
{
	struct mmc_command *cmd = host->cmd;
	u32 cmd_status = host->ops->get_cmd_status(host);

	if (!ali_mmc_get_cd(host->mmc)) {
		dev_warn(mmc_dev(host->mmc), "%s: Card Removed!\n", __func__);
		if (!host->cmd)
			return -ENOMEDIUM;
		host->cmd->error = -ENOMEDIUM;
	} else if (cmd_status & CMD_STATUS_CRC7_ERR) {
		dev_err(mmc_dev(host->mmc), "%s: CMD CRC Error!\n", __func__);
		cmd->error = -EILSEQ;
	} else if (cmd_status & CMD_STATUS_CRC16_ERR) {
		dev_err(mmc_dev(host->mmc), "%s: DATA CRC Error!\n", __func__);
		cmd->error = -EILSEQ;
	}

	return (host->req) ? cmd->error : -ENOMEDIUM;
}

static void ali_mmc_request_done(struct ali_mmc_host *host)
{
	struct mmc_request *req = host->req;

	/* Force SD_CLK Enable */
	host->ops->enable_force_clock(host, 1);

	if (!ali_mmc_get_cd(host->mmc)) {
		if (host->cmd)
			host->cmd->error = -ENOMEDIUM;
	}

	host->req = NULL;
	host->cmd = NULL;

	if(host->pdata->pin_group == 0)
		ali_mmc_pinmux_restore(host->pdata->pin_group);

	if (req == NULL)
		return;

	mmc_request_done(host->mmc, req);
}

static void ali_mmc_watchdog(unsigned long watchdog_data)
{
	struct mmc_host *mmc = (struct mmc_host *)watchdog_data;
	struct ali_mmc_host *host = mmc_priv(mmc);
	struct mmc_request *req = host->req;

	ali_mmc_reset(host);
	if (!req)
		return;

	dev_warn(mmc_dev(host->mmc), "CMD%d Time out ! cmd_status = 0x%.02x\n",
		host->cmd->opcode, host->ops->get_cmd_status(host));
		
	/* Mark transfer as erroneus and inform the upper layers */
	if (req->data) {
		req->data->error = -ETIMEDOUT;
		if (host->use_pio)
			host->ops->pio_cleanup(host);
		else
			dma_unmap_sg(mmc_dev(host->mmc), req->data->sg,
				host->sg_len, host->dma_dir);
	}

	req->cmd->error = -ETIMEDOUT;
	ali_mmc_request_done(host);
}

static void ali_mmc_sdio_timer(unsigned long timer_data)
{
	struct mmc_host *mmc = (struct mmc_host *)timer_data;
	struct ali_mmc_host *host = mmc_priv(mmc);

	if (!host->sdio_irq_enable)
		return;

	if (!host->data_transferring &&
		!(host->ops->get_cmd_status(host) & CMD_STATUS_DATA_SERIN_1)) {
		dev_dbg(mmc_dev(host->mmc), "%s: SDIO_INT\n", __func__);
		mmc_signal_sdio_irq(host->mmc);
	} else
		mod_timer(&host->sdio_timer, jiffies + msecs_to_jiffies(5));
}

static void ali_mmc_prepare_pio(struct ali_mmc_host *host,
	struct mmc_data *data)
{
	host->ops->set_pio(host, (data->flags & MMC_DATA_WRITE)
				? MMC_DATA_WRITE : MMC_DATA_READ);
}

static void ali_mmc_pio_transfer(struct ali_mmc_host *host,
	struct mmc_data *data)
{
	if (data->flags & MMC_DATA_WRITE)
		host->ops->pio_write(host, data);
	else
		host->ops->pio_read(host, data);
}

static void ali_mmc_prepare_dma(struct ali_mmc_host *host,
	struct mmc_data *data)
{
	dma_addr_t dma_addr;

	host->dma_dir = (data->flags & MMC_DATA_WRITE) ?
				DMA_TO_DEVICE : DMA_FROM_DEVICE;

	host->sg_len = dma_map_sg(mmc_dev(host->mmc), data->sg,
				data->sg_len, host->dma_dir);
	dma_addr = sg_phys(data->sg);

	if (!IS_ALIGNED((u32)sg_virt(data->sg), 32) || (data->blksz%32)) {
		if (host->dma_dir == DMA_TO_DEVICE) {
			if(data->blksz%32){
				u32 i=0;
				for(i=0; i<data->blocks; i++)
					memcpy(host->virt_buf+i*32, (u8 *)sg_virt(data->sg)+i*data->blksz,
						data->blksz);
			}	
			else	
				memcpy(host->virt_buf, (u8 *)sg_virt(data->sg),
					host->data_size);
				
		}
		dma_addr = host->phys_buf;
	}
	host->ops->set_dma(host, dma_addr, host->data_size, host->dma_dir);
}

static void ali_mmc_prepare_data(struct ali_mmc_host *host,
	struct mmc_data *data)
{
	if (data == NULL)
		return;

	host->ops->set_block(host, data->blocks, data->blksz);
	host->data_size = data->blocks * data->blksz;
	//host->use_pio = (data->blksz % 32);

	if (host->data_size & 0x1)
		dev_warn(mmc_dev(host->mmc), "%s data_size(%d) is odd.\n",
			__func__, host->data_size);

	if (data->blksz % 32)
		dev_warn(mmc_dev(host->mmc), "%s blksz(%d) is not mutiples of 32.\n",
			__func__, data->blksz);

	if (host->use_pio)
		ali_mmc_prepare_pio(host, data);
	else
		ali_mmc_prepare_dma(host, data);
}

static void ali_mmc_start_command(struct ali_mmc_host *host,
	struct mmc_command *cmd, struct mmc_data *data)
{
	dev_dbg(mmc_dev(host->mmc),
		"start cmd: CMD%d, Arg 0x%.08x, Flags 0x%.08x\n",
		cmd->opcode, cmd->arg, cmd->flags);
		
	if (data) {
		host->data_transferring = true;
		/* Force SD_CLK Disabled */
		host->ops->enable_force_clock(host, 0);
	}

	host->cmd = cmd;
	host->ops->enable_irq(host, data, 1);
	host->ops->set_cmd(host, cmd, data);

	spin_lock(&host->lock);
	host->ops->start_cmd(host);
	spin_unlock(&host->lock);

	if (host->use_pio) {
		spin_lock(&host->lock);
		ali_mmc_pio_transfer(host, data);
		spin_unlock(&host->lock);

		if (cmd->error) {
			host->ops->pio_cleanup(host);
			ali_mmc_reset(host);
			ali_mmc_request_done(host);
			return;
		}
	}

	if (data)
		mod_timer(&host->watchdog, jiffies + WATCH_DOG_DATA_TIMEOUT);
	else
		mod_timer(&host->watchdog, jiffies + WATCH_DOG_CMD_TIMEOUT);		
}

static void ali_mmc_request(struct mmc_host *mmc, struct mmc_request *req)
{
	struct ali_mmc_host *host = mmc_priv(mmc);

	if (!ali_mmc_get_cd(mmc)) {
		req->cmd->error = -ENOMEDIUM;
		mmc_request_done(mmc, req);
		return;
	}

	if(host->pdata->pin_group == 0)
		ali_mmc_pinmux_set(host->pdata->pin_group);

	host->req = req;
	
	if (req->sbc) {
		ali_mmc_start_command(host, req->sbc, NULL);
	} else {
		ali_mmc_prepare_data(host, req->data);
		ali_mmc_start_command(host, req->cmd, req->data);
	}
}

static void ali_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct ali_mmc_host *host = mmc_priv(mmc);

	dev_dbg(mmc_dev(host->mmc), "%s: clock %u Hz, width %u, timing %u\n",
		 __func__, ios->clock, ios->bus_width, ios->timing);

	spin_lock(&host->lock);

	host->clock = ios->clock;
	host->bus_width = ios->bus_width;
	host->timing = ios->timing;

	host->ops->set_clock(host, host->clock);
	host->ops->set_bus_width(host, host->bus_width);
	host->ops->set_timing(host, host->timing);

	spin_unlock(&host->lock);
}

static int ali_mmc_get_ro(struct mmc_host *mmc)
{
	struct ali_mmc_host *host = mmc_priv(mmc);

	if(!gpio_is_valid(host->pdata->wp_gpios))
		return 0;

	if(host->pdata->wp_inverted)
	        return !ali_gpio_get_value(host->pdata->wp_gpios);
	else
	        return ali_gpio_get_value(host->pdata->wp_gpios);
}

static int ali_mmc_get_cd(struct mmc_host *mmc)
{
	struct ali_mmc_host *host = mmc_priv(mmc);
		
	if(mmc->caps & MMC_CAP_NONREMOVABLE)
		return 1;

	if(host->pdata->cd_inverted)
	        return ali_gpio_get_value(host->pdata->cd_gpios);
	else
	        return !ali_gpio_get_value(host->pdata->cd_gpios);
}

static void ali_mmc_enable_sdio_irq(struct mmc_host *mmc, int enable)
{
	struct ali_mmc_host *host = mmc_priv(mmc);

	dev_dbg(mmc_dev(host->mmc), "%s: %s\n",
		__func__, (enable) ? "Enable" : "Disable");
	host->sdio_irq_enable = (enable) ? true : false;

	if (host->sdio_irq_enable)
		mod_timer(&host->sdio_timer, jiffies + msecs_to_jiffies(1));

	spin_lock(&host->lock);
	host->ops->enable_sdio_irq(host, host->sdio_irq_enable);
	spin_unlock(&host->lock);

	return;

}

static void ali_mmc_cmdwork(struct work_struct *work)
{
	struct ali_mmc_host *host = container_of(work, struct ali_mmc_host,
						  cmdwork);
	struct mmc_command *cmd = host->cmd;
	struct mmc_request *req = host->req;

	host->ops->enable_irq(host, NULL, 0);
	if (!ali_mmc_check_status(host))
		host->ops->get_response(host, cmd);

	if (host->cmd25_done_delay_ms < 5 && cmd->opcode == 13 && cmd->resp[0] == 0x00000e00)
		host->cmd25_done_delay_ms++;		
		

#if 0
	/* Multi Block CMD Write and Stop CMD Done */	
	if (host->cmd25_done_delay_ms && req->cmd->opcode ==25 && cmd->opcode == 12)
		mdelay(host->cmd25_done_delay_ms);
#endif    
		
	if (!cmd->error && cmd == req->sbc) {
		ali_mmc_prepare_data(host, req->data);
		ali_mmc_start_command(host, req->cmd, req->data);
	} else {
		ali_mmc_request_done(host);
	}
}

static void ali_mmc_datawork(struct work_struct *work)
{
	struct ali_mmc_host *host = container_of(work, struct ali_mmc_host,
						  datawork);
	struct mmc_command *cmd = host->cmd;
	struct mmc_request *req = host->req;
	struct mmc_data *data = req->data;

	host->ops->enable_irq(host, NULL, 0);
	if (!ali_mmc_check_status(host))
		host->ops->get_response(host, cmd);

	if (host->use_pio) {
		host->ops->pio_cleanup(host);
		ali_mmc_reset(host);
	} else {
		if (cmd && !cmd->error && data && data->sg) {
			data->bytes_xfered += data->sg->length;
			dma_unmap_sg(mmc_dev(host->mmc), data->sg, host->sg_len,
				host->dma_dir);
			if (!cmd->error &&
			    (!IS_ALIGNED((u32)sg_virt(data->sg), 32)|| (data->blksz%32)) &&
			    (host->dma_dir == DMA_FROM_DEVICE)) {
			    	if(data->blksz%32) {
					u32 i=0;
					for(i=0; i<data->blocks; i++)
						memcpy((u8 *) sg_virt(data->sg)+i*data->blksz,
							host->virt_buf+i*32, data->blksz);		    	
				}
				else
					memcpy((u8 *) sg_virt(data->sg),
						host->virt_buf, data->sg->length);
			}
		}
	}

	if (cmd && !cmd->error && data && data->stop)
		ali_mmc_start_command(host, data->stop, NULL);
	else
		ali_mmc_request_done(host);
}

static irqreturn_t ali_mmc_irq(int irq, void *dev_id)
{
	struct ali_mmc_host *host = dev_id;
	u32 int_status = host->ops->get_and_clear_irq(host);

	if (int_status & INT_STATUS_DATA_END) {
		dev_dbg(mmc_dev(host->mmc), "INT_DMA_DATA_END\n");
		host->data_transferring = false;
		del_timer(&host->watchdog);
		schedule_work(&host->datawork);
	}

	if (int_status & INT_STATUS_CMD_END) {
		dev_dbg(mmc_dev(host->mmc), "INT_CMD_END\n");
		del_timer(&host->watchdog);
		schedule_work(&host->cmdwork);
	}

	if (int_status & INT_STATUS_SDIO) {
		dev_dbg(mmc_dev(host->mmc), "SDIO_INT\n");
		del_timer(&host->sdio_timer);
		if (host->sdio_irq_enable)
			mmc_signal_sdio_irq(host->mmc);
	}

	return IRQ_HANDLED;
}

/* for PDK 1.6.2 */
/*
 * ISR for the CardDetect Pin
 */
static irqreturn_t ali_mmc_card_detect_irq(int irq, void *dev_id)
{
	struct ali_mmc_host *host = dev_id;
	int int_status;

	dev_dbg(mmc_dev(host->mmc), "ali_mmc_card_detect_irq!\n");

    	int_status = get_gpio_interrupt_status(host->pdata->cd_gpios);
    	if (1 != int_status)
        	return IRQ_NONE;

	clear_gpio_interrupt_status(host->pdata->cd_gpios);

        mmc_detect_change(host->mmc, 0);

	return IRQ_HANDLED;
}

static void ali_mmc_req_cd_gpio(struct ali_mmc_host *host)
{
	if (gpio_is_valid(host->pdata->cd_gpios)) {

	        if (gpio_request(host->pdata->cd_gpios, "mmc card detect")) {
	        	dev_err(mmc_dev(host->mmc), "gpio_request(%d) failed !\n",
				(int) host->pdata->cd_gpios);
			host->mmc->caps |= MMC_CAP_NONREMOVABLE;
			return;
	        }

	        //gpio rising edge interrupt enable, 1: enable  0: disable
	        set_gpio_rising_ir_pin(host->pdata->cd_gpios, 1);

	        //gpio falling edge interrupt enable, 1: enable  0: disable
		set_gpio_falling_ir_pin(host->pdata->cd_gpios, 1);

		gpio_enable_pin(host->pdata->cd_gpios);
        	ali_gpio_direction_input(host->pdata->cd_gpios);

		if (request_irq(INT_ALI_GPIO, ali_mmc_card_detect_irq, IRQF_SHARED, "mmc card detect", host)) {
  	        	dev_err(mmc_dev(host->mmc), "request_irq(%d) failed !\n",
				(int) host->pdata->cd_gpios);
			host->mmc->caps |= MMC_CAP_NEEDS_POLL;
			return;
            	}
		enable_gpio_interrupt_pin(host->pdata->cd_gpios/*,TRUE,TRUE*/);
	}
}

static void ali_mmc_req_ro_gpio(struct ali_mmc_host *host)
{

	if (gpio_is_valid(host->pdata->wp_gpios)) {
		if (gpio_request(host->pdata->wp_gpios, "mmc card read only"))  {
			dev_err(mmc_dev(host->mmc), "gpio_request(%d) failed !\n",
				(int) host->pdata->wp_gpios);
			return;
		}
		gpio_enable_pin(host->pdata->wp_gpios);
	        ali_gpio_direction_input(host->pdata->wp_gpios);
        }
}

/* end of for PDK 1.6.2 */
static const struct mmc_host_ops ali_mmc_ops = {
	.request	= ali_mmc_request,
	.set_ios	= ali_mmc_set_ios,
	.get_ro		= ali_mmc_get_ro,
	.get_cd		= ali_mmc_get_cd,
	.enable_sdio_irq = ali_mmc_enable_sdio_irq,
};

static const struct of_device_id ali_mmc_dt_ids[] = {
	{ .compatible = "alitech,mmc"},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ali_mmc_dt_ids);

static int ali_mmc_probe(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct ali_mmc_host *host = NULL;
	int ret = 0;

	dev_info(&pdev->dev, "ALi MMC/SD driver\n");

	mmc = mmc_alloc_host(sizeof(struct ali_mmc_host), &pdev->dev);
	if (!mmc) {
		dev_err(&pdev->dev, "mmc_alloc_host failed !\n");
		return -ENOMEM;
	}

	host = mmc_priv(mmc);
	host->mmc = mmc;
	host->pdata = pdev->dev.platform_data;
	host->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	host->irq = platform_get_irq(pdev, 0);
	if (!host->res || host->irq < 0) {
		dev_err(&pdev->dev, "platform_get_resource, irq failed !\n");
		ret = -EINVAL;
		goto out_free_host;
	}

	if (host->pdata == NULL) {
		dev_err(&pdev->dev, "platform data missing\n");
		ret = -ENXIO;
		goto out_free_host;
	}

	host->base = ioremap(host->res->start, resource_size(host->res));
	if (!host->base) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "devm_ioremap_resource failed !\n");
		goto out_free_host;
	}

	host->virt_buf = dma_alloc_coherent(&pdev->dev, BUF_SIZE,
					     &host->phys_buf, GFP_KERNEL);
	if (!host->virt_buf) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "dma_alloc_coherent failed !\n");
		goto out_free_host;
	}

	/* for PDK 1.6.2 */
	if(host->pdata->pin_group != 0)
		ali_mmc_pinmux_set(host->pdata->pin_group);

	ali_mmc_soc_clock_gate(false);

	/* for PDK 1.6.2 */
	mmc->caps = host->pdata->capability | MMC_CAP_4_BIT_DATA | MMC_CAP_SDIO_IRQ | MMC_CAP_SD_HIGHSPEED | MMC_CAP_MMC_HIGHSPEED;
	mmc->f_max = host->pdata->max_frequency;
	ali_mmc_req_cd_gpio(host);
	ali_mmc_req_ro_gpio(host);

	mmc->ops = &ali_mmc_ops;
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->f_min = DIV_ROUND_UP(mmc->f_max, (2*(256-1)));

	mmc->max_blk_count  = 65536;
	mmc->max_blk_size   = 2048;
	mmc->max_req_size   = BUF_SIZE;
	mmc->max_seg_size   = mmc->max_req_size;

	ret = request_irq(host->irq, ali_mmc_irq, 0, DRIVER_NAME, host);
	if (ret) {
		dev_err(&pdev->dev, "request_irq failed !\n");
		goto out_clk_put;
	}

	host->hc_rev_no = ali_mmc_get_hw_rev(host);
	dev_info(&pdev->dev, "host->hc_rev_no = 0x%.08x\n", host->hc_rev_no);

	host->ops = (host->hc_rev_no <= 3) ? &ali_mmc_host_hw_v3_ops : &ali_mmc_host_hw_v4_ops;
	host->cmd25_done_delay_ms = 0;
	
	spin_lock_init(&host->lock);
	INIT_WORK(&host->cmdwork, ali_mmc_cmdwork);
	INIT_WORK(&host->datawork, ali_mmc_datawork);
	setup_timer(&host->watchdog, ali_mmc_watchdog, (unsigned long) mmc);
	setup_timer(&host->sdio_timer, ali_mmc_sdio_timer, (unsigned long) mmc);

	ali_mmc_soc_reset();
	ret = mmc_add_host(mmc);
	if (ret) {
		dev_err(&pdev->dev, "cannot add mmc host\n");
		goto out_free_irq;
	}

	platform_set_drvdata(pdev, mmc);

	return 0;

out_free_irq:
	free_irq(host->irq, host);

out_clk_put:
	clk_disable_unprepare(host->clk);

out_free_dma:
	dma_free_coherent(&pdev->dev, BUF_SIZE, host->virt_buf, host->phys_buf);

out_free_host:
	mmc_free_host(mmc);

	return ret;
}

static int ali_mmc_remove(struct platform_device *pdev)
{
	struct mmc_host *mmc = platform_get_drvdata(pdev);
	struct ali_mmc_host *host = mmc_priv(mmc);

	platform_set_drvdata(pdev, NULL);
	mmc_remove_host(mmc);
	free_irq(host->irq, host);
	clk_disable_unprepare(host->clk);
	dma_free_coherent(&pdev->dev, BUF_SIZE, host->virt_buf, host->phys_buf);
	mmc_free_host(mmc);

	return 0;
}

#ifdef CONFIG_PM
static int ali_mmc_suspend(struct device *dev)
{
	return 0;
}

static int ali_mmc_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops ali_mmc_pm = {
	.suspend	= ali_mmc_suspend,
	.resume		= ali_mmc_resume,
};

#define ali_mmc_pm_ops (&ali_mmc_pm)
#else
#define ali_mmc_pm_ops NULL
#endif

static struct platform_driver ali_mmc_driver = {
	.probe		= ali_mmc_probe,
	.remove		= ali_mmc_remove,
	.driver		= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm	= ali_mmc_pm_ops,
		.of_match_table = of_match_ptr(ali_mmc_dt_ids),
	},
};

module_platform_driver(ali_mmc_driver);

MODULE_ALIAS("platform:ali-mmc");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("eMMC/SD driver for ALi SD/eMMC controller");
MODULE_VERSION("1.0.0");
MODULE_AUTHOR("David Shih <david.shih@alitech.com>, Lucas Lai<lucas.lai@alitech.com>");
