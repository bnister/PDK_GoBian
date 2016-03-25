/*
 * mmc-ali-hw-v4.c - MMC/SD/SDIO driver for ALi SoCs
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

#include "mmc-ali.h"

#define REG_CMD_CTRL	0x00	/* CMD Control Register */
#define		CMD_START	(1 << 7)
#define		RESP_TYPE_R0	(0 << 4)	/* No Response Data */
#define		RESP_TYPE_R1	(1 << 4)	/* 4 bytes response data */
#define		RESP_TYPE_R2	(2 << 4)	/* 16 bytes response data */
#define		RESP_TYPE_R3	(3 << 4)	/* 4 bytes response data */
#define		RESP_TYPE_R6	(4 << 4)	/* 4 bytes response data */
#define		CMD_TYPE_BC	(0 << 0)
#define		CMD_TYPE_BCR	(1 << 0)
#define		CMD_TYPE_AC	(2 << 0)
#define		CMD_TYPE_ADTC_RD	(4 << 0)
#define		CMD_TYPE_ADTC_WR	(5 << 0)

#define REG_CMD_INDEX	0x01	/* CMD Index Register */
#define REG_STATUS	0x02	/* CMD Status Register */
#define		STATUS_DATA_SERIN_3	(1 << 7)
#define		STATUS_DATA_SERIN_2	(1 << 6)
#define		STATUS_DATA_SERIN_1	(1 << 5)
#define		STATUS_DATA_SERIN_0	(1 << 4)
#define		STATUS_DATA_BUSY	(1 << 3)
#define		STATUS_CMD_BUSY		(1 << 2)
#define		STATUS_CRC16_ERR	(1 << 1)
#define		STATUS_CRC7_ERR		(1 << 0)

#define REG_ARGUMENT	0x04	/* Argument Register */
#define REG_BLK_LEN	0x08	/* Block Length Register */
#define REG_BLK_NUM	0x0A	/* Block Num Register */

#define REG_PIO_BUF	0x0C	/* PIO RD/WR Buffer Register */
#define REG_PIO_CTRL	0x0E	/* PIO Control Register */
#define		PIO_DIR_RX	(0 << 3)
#define		PIO_DIR_TX	(1 << 3)
#define		PIO_ENABLE	(1 << 2)
#define		PIO_BUF_EMPTY	(1 << 1)
#define		PIO_CLR_DATA	(1 << 0)

#define REG_RESPONSE_0	0x10	/* Response[31:0] */
#define REG_RESPONSE_1	0x14	/* Response[63:32] */
#define REG_RESPONSE_2	0x18	/* Response[95:64] */
#define REG_RESPONSE_3	0x1C	/* Response[127:96] */

#define REG_DMA_RX_ADDR	0x20	/* DMA Rx Address Register */
#define REG_DMA_TX_ADDR	0x24	/* DMA Tx Address Register */

#define REG_DMA_CTRL	0x30	/* DMA Control Register */
#define		DMA_BUSY	(1 << 4)
#define		DMA_TX_DONE	(1 << 3)
#define		DMA_RX_DONE	(1 << 2)
#define		DMA_DIR_RX	(0 << 1)
#define		DMA_DIR_TX	(1 << 1)
#define		DMA_START	(1 << 0)

#define REG_INT_ENABLE	0x34
#define REG_INT_STATUS	0x36
#define		INT_DMA_TX_BUS_IDLE	(1 << 2)
#define		INT_DMA_DATA_END	(1 << 1)
#define		INT_CMD_END		(1 << 0)

#define REG_CLK_DIV	0x40	/* Clock Frequency Divider Register */
#define REG_CLK_CTRL	0x42
#define		CLK_FORCE_ENABLE	(1 << 0)

#define REG_CLK_IN_DELAY	0x44	/* Clock Input Delay Register */
#define REG_CLK_OUT_DELAY	0x45	/* Clock Output Delay Register */

#define REG_SDIO_CTRL	0x48	/* SDIO Card Control Register */
#define		SDIO_INT_EN		(1 << 1)
#define		SDIO_MODE_EN		(1 << 0)

#define REG_SDIO_STATUS	0x49	/* SDIO Card Control Register */
#define		SDIO_INT_STATUS		(1 << 2)
#define		SDIO_READ_WAIT		(1 << 1)
#define		SDIO_STOP_WC		(1 << 0)

#define REG_HC_VERSION	0x4C	/* Host Control Rev Register */

#define REG_BUS_WIDTH_SEL	0x50	/* Bus Width Selection Register */
#define		BUS_WIDTH_1_BIT		(0 << 0)
#define		BUS_WIDTH_4_BIT		(1 << 0)
#define		BUS_WIDTH_8_BIT		(2 << 0)

#define REG_BUS_MODE	0x51	/* SD Bus Mode Register */
#define		BUS_MODE_DS_HS		(0 << 0)
#define		BUS_MODE_UHS_I		(1 << 0)
#define		BUS_MODE_DDR50		(2 << 0)

#define REG_PHY_CTRL	0x54
#define REG_SRAM_CTRL	0x58

static void ali_mmc_host_hw_v4_set_clock(struct ali_mmc_host *host,
	unsigned int clock)
{
	if (clock == 0)
		ali_mmc_writew(host, 0, REG_CLK_DIV);
	else if (clock == host->mmc->f_max)
		ali_mmc_writew(host, 1, REG_CLK_DIV);
	else
		ali_mmc_writew(host, DIV_ROUND_UP(host->mmc->f_max, 2*clock)+1,
			REG_CLK_DIV);
}

static void ali_mmc_host_hw_v4_set_bus_width(struct ali_mmc_host *host,
	unsigned char bus_width)
{
	switch (bus_width) {
	case MMC_BUS_WIDTH_1:
		ali_mmc_writeb(host, BUS_WIDTH_1_BIT, REG_BUS_WIDTH_SEL);
		break;
	case MMC_BUS_WIDTH_4:
		ali_mmc_writeb(host, BUS_WIDTH_4_BIT, REG_BUS_WIDTH_SEL);
		break;
	case MMC_BUS_WIDTH_8:
		ali_mmc_writeb(host, BUS_WIDTH_8_BIT, REG_BUS_WIDTH_SEL);
		break;
	}
}

static void ali_mmc_host_hw_v4_set_timing(struct ali_mmc_host *host,
	unsigned char timing)
{
	switch (timing)	{
	case MMC_TIMING_LEGACY:
	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
		ali_mmc_writeb(host, BUS_MODE_DS_HS, REG_BUS_MODE);
		break;
//	case MMC_TIMING_UHS_SDR12:
//	case MMC_TIMING_UHS_SDR25:
	case MMC_TIMING_UHS_SDR50:
	case MMC_TIMING_UHS_SDR104:
		ali_mmc_writeb(host, BUS_MODE_UHS_I, REG_BUS_MODE);
		break;
	case MMC_TIMING_UHS_DDR50:
		ali_mmc_writeb(host, BUS_MODE_DDR50, REG_BUS_MODE);
		break;
	default:
		dev_warn(mmc_dev(host->mmc), "%s: Unsupport Timing (%d)\n",
				__func__, timing);
		break;
	}
}

static void ali_mmc_host_hw_v4_enable_force_clock(struct ali_mmc_host *host,
	int enable)
{
	ali_mmc_writeb(host, (enable) ? CLK_FORCE_ENABLE : 0,
		REG_CLK_CTRL);
}

static u8 ali_mmc_cmd_type(struct mmc_command *cmd, struct mmc_data *data)
{
	switch (mmc_cmd_type(cmd)) {
	case MMC_CMD_BC:	return CMD_TYPE_BC;
	case MMC_CMD_BCR:	return CMD_TYPE_BCR;
	case MMC_CMD_AC:	return CMD_TYPE_AC;
	case MMC_CMD_ADTC:
		return (data->flags & MMC_DATA_WRITE) ?
				CMD_TYPE_ADTC_WR : CMD_TYPE_ADTC_RD;
	default:
		BUG();
	}
}

static u8 ali_mmc_resp_type(struct mmc_command *cmd)
{
	switch (mmc_resp_type(cmd)) {
	case MMC_RSP_NONE:	return RESP_TYPE_R0;
	case MMC_RSP_R1:
	case MMC_RSP_R1B:
		/* In Linux define, MMC_RSP_R1 is equal to MMC_RSP_R6 */
		/* MMC_RSP_R6 is used only for SD_SEND_RELATIVE_ADDR cmmand */
		return (cmd->opcode == SD_SEND_RELATIVE_ADDR) ?
				RESP_TYPE_R6 : RESP_TYPE_R1;
	case MMC_RSP_R2:	return RESP_TYPE_R2;
	case MMC_RSP_R3:	return RESP_TYPE_R3;
	default:
		BUG();
	}
}

static void ali_mmc_host_hw_v4_set_cmd(struct ali_mmc_host *host,
	struct mmc_command *cmd, struct mmc_data *data)
{
	ali_mmc_writel(host, cmd->arg, REG_ARGUMENT);
	ali_mmc_writeb(host, 0x40 | ((u8) cmd->opcode), REG_CMD_INDEX);
	ali_mmc_writeb(host, ali_mmc_cmd_type(cmd, data)
		| ali_mmc_resp_type(cmd), REG_CMD_CTRL);
}

static void ali_mmc_host_hw_v4_start_cmd(struct ali_mmc_host *host)
{
	ali_mmc_writeb(host, CMD_START | ali_mmc_readb(host, REG_CMD_CTRL),
		REG_CMD_CTRL);
}

static u32 ali_mmc_host_hw_v4_get_cmd_status(struct ali_mmc_host *host)
{
	u8 reg_val = ali_mmc_readb(host, REG_STATUS);
	u32 cmd_status = 0;

	if (reg_val & STATUS_CRC7_ERR)
		cmd_status |= CMD_STATUS_CRC7_ERR;
	if (reg_val & STATUS_CRC16_ERR)
		cmd_status |= CMD_STATUS_CRC16_ERR;
	if (reg_val & STATUS_CMD_BUSY)
		cmd_status |= CMD_STATUS_CMD_BUSY;
	if (reg_val & STATUS_DATA_BUSY)
		cmd_status |= CMD_STATUS_DATA_BUSY;
	if (reg_val & STATUS_DATA_SERIN_0)
		cmd_status |= CMD_STATUS_DATA_SERIN_0;
	if (reg_val & STATUS_DATA_SERIN_1)
		cmd_status |= CMD_STATUS_DATA_SERIN_1;
	if (reg_val & STATUS_DATA_SERIN_2)
		cmd_status |= CMD_STATUS_DATA_SERIN_2;
	if (reg_val & STATUS_DATA_SERIN_3)
		cmd_status |= CMD_STATUS_DATA_SERIN_3;
	return cmd_status;
}

static void ali_mmc_host_hw_v4_get_response(struct ali_mmc_host *host,
	struct mmc_command *cmd)
{
	if (!(cmd->flags & MMC_RSP_PRESENT))
		return;

	if (cmd->flags & MMC_RSP_136) {
		/* response type 2 */
		cmd->resp[3] = ali_mmc_readl(host, REG_RESPONSE_0);
		cmd->resp[2] = ali_mmc_readl(host, REG_RESPONSE_1);
		cmd->resp[1] = ali_mmc_readl(host, REG_RESPONSE_2);
		cmd->resp[0] = ali_mmc_readl(host, REG_RESPONSE_3);
	} else {
		/* response types 1, 1b, 3, 4, 5, 6 */
		cmd->resp[0] = ((ali_mmc_readl(host, REG_RESPONSE_1) << (32-8))
				& 0xFF000000)
			      |((ali_mmc_readl(host, REG_RESPONSE_0) >> 8)
				& 0x00FFFFFF);
	}
}

static void ali_mmc_host_hw_v4_set_block(struct ali_mmc_host *host,
	unsigned int num_of_blocks, unsigned int block_length)
{
	ali_mmc_writew(host, num_of_blocks-1, REG_BLK_NUM);
	ali_mmc_writew(host, block_length, REG_BLK_LEN);
}

static void ali_mmc_host_hw_v4_set_dma(struct ali_mmc_host *host,
	dma_addr_t addr, unsigned int length,
	enum dma_data_direction direction)
{
	if (direction == DMA_TO_DEVICE) {
		ali_mmc_writel(host, addr, REG_DMA_TX_ADDR);
		ali_mmc_writel(host, DMA_DIR_TX | DMA_START, REG_DMA_CTRL);
	} else {
		ali_mmc_writel(host, addr, REG_DMA_RX_ADDR);
		ali_mmc_writel(host, DMA_DIR_RX | DMA_START, REG_DMA_CTRL);
	}
}

static void ali_mmc_host_hw_v4_set_pio(struct ali_mmc_host *host,
	u32 direction)
{
	u16 pio_ctrl = (direction == MMC_DATA_WRITE) ? PIO_DIR_TX : PIO_DIR_RX;

	ali_mmc_writew(host, pio_ctrl | PIO_ENABLE, REG_PIO_CTRL);
}

static void ali_mmc_host_hw_v4_pio_write(struct ali_mmc_host *host,
	struct mmc_data *data)
{
	u16 *buf = (u16 *)sg_virt(data->sg);
	u32 retry = 0;

	while (data->bytes_xfered < host->data_size) {
		ali_mmc_writew(host, *buf++, REG_PIO_BUF);
		data->bytes_xfered += 2;

		retry = 0;
		while (!(ali_mmc_readw(host, REG_PIO_CTRL) & PIO_BUF_EMPTY) &&
			(retry++ < PIO_TRY_BOUND))
			udelay(1);

		if (retry == PIO_TRY_BOUND) {
			dev_err(mmc_dev(host->mmc),
				"%s: wait buff empty failed\n", __func__);
			host->req->cmd->error = -ETIMEDOUT;
			return;
		}

		if (ali_mmc_host_hw_v4_get_cmd_status(host)
			& CMD_STATUS_CRC16_ERR)
			return;
	}
}

static void ali_mmc_host_hw_v4_pio_read(struct ali_mmc_host *host,
	struct mmc_data *data)
{
	u16 *buf = (u16 *)sg_virt(data->sg);
	u32 retry = 0;

	while (data->bytes_xfered < host->data_size) {
		retry = 0;
		while ((ali_mmc_readw(host, REG_PIO_CTRL) & PIO_BUF_EMPTY) &&
			(retry++ < PIO_TRY_BOUND))
			udelay(1);

		if (retry == PIO_TRY_BOUND) {
			dev_err(mmc_dev(host->mmc),
				"%s: wait buff not empty failed\n", __func__);
			host->req->cmd->error = -ETIMEDOUT;
			return;
		}

		if (ali_mmc_host_hw_v4_get_cmd_status(host)
			& CMD_STATUS_CRC16_ERR)
			return;

		*buf++ = ali_mmc_readw(host, REG_PIO_BUF);
		data->bytes_xfered += 2;
	}
}

static void ali_mmc_host_hw_v4_pio_cleanup(struct ali_mmc_host *host)
{
	host->use_pio = false;

	ali_mmc_writew(host, ali_mmc_readw(host, REG_PIO_CTRL) & ~PIO_ENABLE,
		REG_PIO_CTRL);

	ali_mmc_writew(host, ali_mmc_readw(host, REG_PIO_CTRL) | PIO_CLR_DATA,
		REG_PIO_CTRL);
	udelay(1);
	ali_mmc_writew(host, ali_mmc_readw(host, REG_PIO_CTRL) & ~PIO_CLR_DATA,
		REG_PIO_CTRL);
}

static void ali_mmc_host_hw_v4_enable_irq(struct ali_mmc_host *host,
	struct mmc_data *data, int enable)
{
	if (enable)
		ali_mmc_writew(host, (data) ? INT_DMA_DATA_END : INT_CMD_END,
				REG_INT_ENABLE);
	else
		ali_mmc_writew(host, 0,	REG_INT_ENABLE);
}

static void ali_mmc_host_hw_v4_enable_sdio_irq(struct ali_mmc_host *host,
	int enable)
{
	ali_mmc_writeb(host, (enable) ? (SDIO_INT_EN|SDIO_MODE_EN) : 0,
		REG_SDIO_CTRL);
}

static u32 ali_mmc_host_hw_v4_get_and_clear_irq(struct ali_mmc_host *host)
{
	u16 reg_int_val = ali_mmc_readw(host, REG_INT_STATUS);
	u8 reg_sdio_val = ali_mmc_readb(host, REG_SDIO_STATUS);
	u32 int_status = 0;

	/* Get irq status */
	if (reg_int_val & INT_CMD_END)
		int_status |= INT_STATUS_CMD_END;
	if (reg_int_val & INT_DMA_DATA_END)
		int_status |= INT_STATUS_DATA_END;
	if (reg_sdio_val & SDIO_INT_STATUS)
		int_status |= INT_STATUS_SDIO;

	/* Clear irq */
	ali_mmc_writew(host, reg_int_val, REG_INT_STATUS);
	ali_mmc_writeb(host, reg_sdio_val, REG_SDIO_STATUS);

	return int_status;
}

const struct ali_mmc_host_ops ali_mmc_host_hw_v4_ops = {
	.set_clock	= ali_mmc_host_hw_v4_set_clock,
	.set_bus_width	= ali_mmc_host_hw_v4_set_bus_width,
	.set_timing	= ali_mmc_host_hw_v4_set_timing,
	.enable_force_clock = ali_mmc_host_hw_v4_enable_force_clock,
	.set_cmd	= ali_mmc_host_hw_v4_set_cmd,
	.start_cmd	= ali_mmc_host_hw_v4_start_cmd,
	.get_cmd_status	= ali_mmc_host_hw_v4_get_cmd_status,
	.get_response	= ali_mmc_host_hw_v4_get_response,
	.set_block	= ali_mmc_host_hw_v4_set_block,
	.set_dma	= ali_mmc_host_hw_v4_set_dma,
	.set_pio	= ali_mmc_host_hw_v4_set_pio,
	.pio_write	= ali_mmc_host_hw_v4_pio_write,
	.pio_read	= ali_mmc_host_hw_v4_pio_read,
	.pio_cleanup	= ali_mmc_host_hw_v4_pio_cleanup,
	.enable_irq	= ali_mmc_host_hw_v4_enable_irq,
	.enable_sdio_irq = ali_mmc_host_hw_v4_enable_sdio_irq,
	.get_and_clear_irq = ali_mmc_host_hw_v4_get_and_clear_irq,
};
