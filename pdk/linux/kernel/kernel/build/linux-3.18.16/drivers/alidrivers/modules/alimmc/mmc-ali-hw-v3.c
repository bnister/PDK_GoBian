/*
 * mmc-ali-hw-v3.c - MMC/SD/SDIO driver for ALi SoCs
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

#define REG_CMD_CTRL		0x00		/* CMD Control Register */
#define		MODE_SD_4BIT		(1 << 7)
#define		MODE_SD_1BIT_EMMC	(0 << 7)
#define		RESP_TYPE_R0		(0 << 4) /* No Response Data */
#define		RESP_TYPE_R1		(1 << 4) /* 4 bytes response data */
#define		RESP_TYPE_R2		(2 << 4) /* 16 bytes response data */
#define		RESP_TYPE_R3		(3 << 4) /* 4 bytes response data */
#define		RESP_TYPE_R6		(4 << 4) /* 4 bytes response data */
#define		CMD_TYPE_BC		(0 << 1)
#define		CMD_TYPE_BCR		(1 << 1)
#define		CMD_TYPE_AC		(2 << 1)
#define		CMD_TYPE_ADTC_RD	(4 << 1)
#define		CMD_TYPE_ADTC_WR	(5 << 1)
#define		CMD_START		(1 << 0)

#define REG_STATUS		0x01		/* CMD Status Register */
#define		STATUS_DATA_SERIN_3	(1 << 7)
#define		STATUS_DATA_SERIN_0	(1 << 6)
#define		STATUS_DATA_SERIN_2	(1 << 5)
#define		STATUS_DATA_BUSY	(1 << 4)
#define		STATUS_CMD_BUSY		(1 << 3)
#define		STATUS_DATA_SERIN_1	(1 << 2)
#define		STATUS_CRC16_ERR	(1 << 1)
#define		STATUS_CRC7_ERR		(1 << 0)

#define REG_CMD_INDEX		0x02		/* CMD Index Register */
#define		PIO_DIR_RX		(0 << 7)
#define		PIO_DIR_TX		(1 << 7)
#define		SD_INT_EN		(1 << 6)
#define		CMD_IDX_MASK		(0x3f << 0) /* 6 bits, CMD Index */

#define REG_CLK_DIV_L		0x03		/* Clock Divider Low 8 bit */
#define REG_CLK_DIV_H		0x34		/* Clock Divider High 8 bit */

#define REG_ARGUMENT		0x04		/* Argument Register */
#define REG_BLK_LEN		0x08		/* Block Length Register */
#define REG_BLK_NUM_L		0x0A		/* Block Num Low 8 bit */
#define REG_BLK_NUM_H		0x36		/* Block Num High 8 bit */

#define REG_BUS_WIDTH_FORCE_CLK	0x0B
#define		SDIO_4BIT		(1 << 3)
#define		SDIO_1BIT		(1 << 2)
#define		MMC_8BIT		(1 << 1)
#define		BUS_WIDTH_MASK		(MMC_8BIT|SDIO_1BIT|SDIO_4BIT)
#define		CLK_FORCE_ENABLE	(1 << 0)

#define REG_PIO_BUF		0x0C		/* PIO RD/WR Buffer Register */
#define REG_PIO_CTRL		0x0E		/* PIO Control Register */
#define		PIO_ENABLE		(1 << 2)
#define		PIO_BUF_EMPTY		(1 << 1)
#define		PIO_CLR_DATA		(1 << 0)

#define REG_SDIO_CTRL		0x0F		/* SDIO Control Register */
#define		SDIO_INT_FLG		(1 << 3)
#define		SDIO_INT_EN		(1 << 2)

#define REG_RESPONSE_0		0x10		/* Response[31:0] */
#define REG_RESPONSE_1		0x14		/* Response[63:32] */
#define REG_RESPONSE_2		0x18		/* Response[95:64] */
#define REG_RESPONSE_3		0x1C		/* Response[127:96] */

#define REG_DMA_RX_ADDR		0x20		/* DMA Rx Address Register */
#define REG_DMA_TX_ADDR		0x24		/* DMA Tx Address Register */
#define REG_DMA_RX_SIZE		0x28		/* DMA Rx Size Register */
#define REG_DMA_TX_SIZE		0x2C		/* DMA Tx Size Register */

#define REG_DMA_CTRL		0x30		/* DMA Control Register */
#define		SDIO_INT_STATUS		(1 << 7)
#define		SYS_INT_STATUS		(1 << 6)
#define		DMA_SW_RST		(1 << 5)
#define		DMA_BUSY		(1 << 4)
#define		DMA_TX_DONE		(1 << 3)
#define		DMA_RX_DONE		(1 << 2)
#define		DMA_DIR_RX		(0 << 1)
#define		DMA_DIR_TX		(1 << 1)
#define		DMA_START		(1 << 0)

#define REG_HC_VERSION		0x48		/* Host Control Rev Register */

#define REG_BUS_MODE		0x50		/* SD Bus Mode Register */
#define		BUS_MODE_DS_HS		(0 << 0)
#define		BUS_MODE_UHS_I_SDR12	(1 << 0)
#define		BUS_MODE_UHS_I_SDR25	(2 << 0)
#define		BUS_MODE_UHS_I_SDR50	(3 << 0)
#define		BUS_MODE_UHS_I_SDR104	(4 << 0)
#define		BUS_MODE_UHS_I_DDR50	(5 << 0)

#define LOW_BYTE(w)	((w)&0x00FF)
#define HIGH_BYTE(w)	(((w)&0xFF00)>>8)

static void ali_mmc_host_hw_v3_set_clock(struct ali_mmc_host *host,
	unsigned int clock)
{
    host->mmc->actual_clock = 0;
	if (clock == 0) {
		ali_mmc_writeb(host, 0, REG_CLK_DIV_L);
		ali_mmc_writeb(host, 0, REG_CLK_DIV_H);
        host->mmc->actual_clock = 0;
	} else if (clock == host->mmc->f_max) {
		ali_mmc_writeb(host, 1, REG_CLK_DIV_L);
		ali_mmc_writeb(host, 0, REG_CLK_DIV_H);
        host->mmc->actual_clock = host->mmc->f_max;
	} else {
		u16 clock_div = DIV_ROUND_UP(host->mmc->f_max, 2*clock)+1;

		ali_mmc_writeb(host, LOW_BYTE(clock_div), REG_CLK_DIV_L);
		ali_mmc_writeb(host, HIGH_BYTE(clock_div), REG_CLK_DIV_H);
        host->mmc->actual_clock = DIV_ROUND_UP(host->mmc->f_max, (2*(clock_div-1)));
	}
}

static void ali_mmc_host_hw_v3_set_bus_width(struct ali_mmc_host *host,
	unsigned char bus_width)
{
	u8 reg_val = ali_mmc_readb(host, REG_BUS_WIDTH_FORCE_CLK);

	switch (bus_width) {
	case MMC_BUS_WIDTH_1:
		reg_val = (reg_val & ~BUS_WIDTH_MASK) | SDIO_1BIT;
		break;
	case MMC_BUS_WIDTH_4:
		reg_val = (reg_val & ~BUS_WIDTH_MASK) | SDIO_4BIT;
		break;
	case MMC_BUS_WIDTH_8:
		reg_val = (reg_val & ~BUS_WIDTH_MASK) | MMC_8BIT;
		break;
	}

	ali_mmc_writeb(host, reg_val, REG_BUS_WIDTH_FORCE_CLK);
}

static void ali_mmc_host_hw_v3_set_timing(struct ali_mmc_host *host,
	unsigned char timing)
{
	switch (timing)	{
	case MMC_TIMING_LEGACY:
	case MMC_TIMING_MMC_HS:
	case MMC_TIMING_SD_HS:
		ali_mmc_writeb(host, BUS_MODE_DS_HS, REG_BUS_MODE);
		break;
//	case MMC_TIMING_UHS_SDR12:
//		ali_mmc_writeb(host, BUS_MODE_UHS_I_SDR12, REG_BUS_MODE);
//		break;
//	case MMC_TIMING_UHS_SDR25:
//		ali_mmc_writeb(host, BUS_MODE_UHS_I_SDR25, REG_BUS_MODE);
//		break;
	case MMC_TIMING_UHS_SDR50:
		ali_mmc_writeb(host, BUS_MODE_UHS_I_SDR50, REG_BUS_MODE);
		break;
	case MMC_TIMING_UHS_SDR104:
		ali_mmc_writeb(host, BUS_MODE_UHS_I_SDR104, REG_BUS_MODE);
		break;
	case MMC_TIMING_UHS_DDR50:
		ali_mmc_writeb(host, BUS_MODE_UHS_I_DDR50, REG_BUS_MODE);
		break;
	default:
		dev_warn(mmc_dev(host->mmc), "%s: Unsupport Timing (%d)\n",
				__func__, timing);
		break;
	}
}

static void ali_mmc_host_hw_v3_enable_force_clock(struct ali_mmc_host *host,
	int enable)
{
	u8 reg_val = ali_mmc_readb(host, REG_BUS_WIDTH_FORCE_CLK);

	ali_mmc_writeb(host, (enable) ?
		(reg_val | CLK_FORCE_ENABLE) : (reg_val & ~CLK_FORCE_ENABLE)
		, REG_BUS_WIDTH_FORCE_CLK);
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

static void ali_mmc_host_hw_v3_set_cmd(struct ali_mmc_host *host,
	struct mmc_command *cmd, struct mmc_data *data)
{
	u8 reg_val;

	/* Setup Command Argument */
	ali_mmc_writel(host, cmd->arg, REG_ARGUMENT);

	/* Setup Command Index */
	reg_val = ali_mmc_readb(host, REG_CMD_INDEX);
	reg_val = (reg_val & ~CMD_IDX_MASK) | ((u8) cmd->opcode);
	ali_mmc_writeb(host, reg_val, REG_CMD_INDEX);

	/* Setup Transfer Mode, Command Type and Response Type */
	reg_val = ((host->bus_width == MMC_BUS_WIDTH_4) ?
			MODE_SD_4BIT : MODE_SD_1BIT_EMMC)
		  | ali_mmc_cmd_type(cmd, data) | ali_mmc_resp_type(cmd);

	ali_mmc_writeb(host, reg_val, REG_CMD_CTRL);
}

static void ali_mmc_host_hw_v3_start_cmd(struct ali_mmc_host *host)
{
	ali_mmc_writeb(host, CMD_START | ali_mmc_readb(host, REG_CMD_CTRL),
		REG_CMD_CTRL);
}

static u32 ali_mmc_host_hw_v3_get_cmd_status(struct ali_mmc_host *host)
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

#define UNSTUFF_BITS(resp,start,size)                                   \
        ({                                                              \
                const int __size = size;                                \
                const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1; \
                const int __off = 3 - ((start) / 32);                   \
                const int __shft = (start) & 31;                        \
                u32 __res;                                              \
                                                                        \
                __res = resp[__off] >> __shft;                          \
                if (__size + __shft > 32)                               \
                        __res |= resp[__off-1] << ((32 - __shft) % 32); \
                __res & __mask;                                         \
        })

static void ali_mmc_host_hw_v3_R2_patch(struct mmc_command *cmd)
{
	/* MMC Host HW Response R2 miss [55:48] 
	 *                 resp[0]  resp[1]  resp[2]  resp[3]   
	 * Should get R2 0xOOOOOOOO OOOOOOOO OO**#### ######## (miss **)
	 * But HW get R2 0xOOOOOOOO OOOOOOOO OO###### ######@@ ([47:00] left shift 8bit)
	 */
	u8 bit_55_48 = 0;
	u8 bit_55_50 = (UNSTUFF_BITS(cmd->resp, 56, 6) << 2); 	/* Copy VDD_W_CURR from VDD_R_CURR */
        u8 csd_structure = UNSTUFF_BITS(cmd->resp, 126, 2);
        u8 emmc_spec_ver = UNSTUFF_BITS(cmd->resp, 122, 4);
                
        /* For CMD9 Respone, try to recovey back bit_55_44 */
	if (cmd->opcode == 9) { 
		if (emmc_spec_ver != 0) {
			/* for eMMC */
			bit_55_48 = bit_55_50 | 0x3;				
		} else if ( csd_structure == 0x00) {
			/* for SD 1.0 */
			u16 c_size = UNSTUFF_BITS(cmd->resp, 62, 12);
				      	
			if (c_size == 0xF4F || c_size == 0xF33 || c_size == 0xF13 || c_size ==0xF03) 
				bit_55_48 = bit_55_50 | 0x02; /* 128 M & 256 M */
			else 	
				bit_55_48 = bit_55_50 | 0x03; /* >= 512MB */      	
		} else if ( csd_structure == 0x01) {
			/* for SD 2.0 */
			bit_55_48 = 0x00;			
		}			
	}	
	
	/* R shift [39:8]->[31:0] */
	cmd->resp[3] = UNSTUFF_BITS(cmd->resp, 8, 32);
		      
	/* Keep [63:56], Recovey [55:48], R shift [55:40]->[47:32] */
	cmd->resp[2] = (cmd->resp[2] & 0xFF000000)
		     | (bit_55_48 << (48-32))
		     | UNSTUFF_BITS(cmd->resp, 40, 16);	    
}

static void ali_mmc_host_hw_v3_get_response(struct ali_mmc_host *host,
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
		ali_mmc_host_hw_v3_R2_patch(cmd);							
	} else {
		/* response types 1, 1b, 3, 4, 5, 6 */
		cmd->resp[0] = ((ali_mmc_readl(host, REG_RESPONSE_1) << (32-8))
				& 0xFF000000)
			      |((ali_mmc_readl(host, REG_RESPONSE_0) >> 8)
				& 0x00FFFFFF);
	}
}

static void ali_mmc_host_hw_v3_set_block(struct ali_mmc_host *host,
	unsigned int num_of_blocks, unsigned int block_length)
{
	ali_mmc_writeb(host, LOW_BYTE(num_of_blocks-1), REG_BLK_NUM_L);
	ali_mmc_writeb(host, HIGH_BYTE(num_of_blocks-1), REG_BLK_NUM_H);
	ali_mmc_writew(host, block_length, REG_BLK_LEN);
}

static void ali_mmc_host_hw_v3_set_dma(struct ali_mmc_host *host,
	dma_addr_t addr, unsigned int length,
	enum dma_data_direction direction)
{
	if (direction == DMA_TO_DEVICE) {
		ali_mmc_writel(host, addr, REG_DMA_TX_ADDR);
		ali_mmc_writel(host, length, REG_DMA_TX_SIZE);
		ali_mmc_writel(host, DMA_DIR_TX | DMA_START, REG_DMA_CTRL);
	} else {
		ali_mmc_writel(host, addr, REG_DMA_RX_ADDR);
		ali_mmc_writel(host, length, REG_DMA_RX_SIZE);
		ali_mmc_writel(host, DMA_DIR_RX | DMA_START, REG_DMA_CTRL);
	}
}

static void ali_mmc_host_hw_v3_set_pio(struct ali_mmc_host *host, u32 direction)
{
	u8 reg_val;

	/* Setup PIO Direction */
	reg_val = (ali_mmc_readb(host, REG_CMD_INDEX) & ~0x80)
		  | ((direction == MMC_DATA_WRITE) ? PIO_DIR_TX : PIO_DIR_RX);
	ali_mmc_writeb(host, reg_val, REG_CMD_INDEX);

	/* Enable PIO Mode */
	ali_mmc_writeb(host, PIO_ENABLE, REG_PIO_CTRL);
}

static void ali_mmc_host_hw_v3_pio_write(struct ali_mmc_host *host,
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

		if (ali_mmc_host_hw_v3_get_cmd_status(host)
			& CMD_STATUS_CRC16_ERR)
			return;
	}
}

static void ali_mmc_host_hw_v3_pio_read(struct ali_mmc_host *host,
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

		if (ali_mmc_host_hw_v3_get_cmd_status(host)
			& CMD_STATUS_CRC16_ERR)
			return;

		*buf++ = ali_mmc_readw(host, REG_PIO_BUF);
		data->bytes_xfered += 2;
	}
}

static void ali_mmc_host_hw_v3_pio_cleanup(struct ali_mmc_host *host)
{
	host->use_pio = false;

	ali_mmc_writeb(host, ali_mmc_readb(host, REG_PIO_CTRL) & ~PIO_ENABLE,
		REG_PIO_CTRL);

	ali_mmc_writeb(host, ali_mmc_readb(host, REG_PIO_CTRL) | PIO_CLR_DATA,
		REG_PIO_CTRL);
	udelay(1);
	ali_mmc_writeb(host, ali_mmc_readb(host, REG_PIO_CTRL) & ~PIO_CLR_DATA,
		REG_PIO_CTRL);
}

static void ali_mmc_host_hw_v3_enable_irq(struct ali_mmc_host *host,
	struct mmc_data *data, int enable)
{
	u8 reg_val = ali_mmc_readb(host, REG_CMD_INDEX);

	if (enable)
		ali_mmc_writeb(host, reg_val | SD_INT_EN, REG_CMD_INDEX);
	else
		ali_mmc_writeb(host, reg_val & ~SD_INT_EN, REG_CMD_INDEX);
}

static void ali_mmc_host_hw_v3_enable_sdio_irq(struct ali_mmc_host *host,
	int enable)
{
	ali_mmc_writeb(host, (enable) ? SDIO_INT_EN : 0, REG_SDIO_CTRL);
}

static u32 ali_mmc_host_hw_v3_get_and_clear_irq(struct ali_mmc_host *host)
{
	u8 reg_int_val = ali_mmc_readb(host, REG_DMA_CTRL);
	u32 int_status = 0;

	/* Clear irq */
	ali_mmc_writeb(host, reg_int_val, REG_DMA_CTRL);

	/* Get irq status */
	if (reg_int_val & SYS_INT_STATUS) {
		if (ali_mmc_readb(host, REG_CMD_CTRL) & (0x01<<3))
			int_status |= INT_STATUS_DATA_END;
		else
			int_status |= INT_STATUS_CMD_END;
	}
	if (reg_int_val & SDIO_INT_STATUS)
		int_status |= INT_STATUS_SDIO;

	return int_status;
}

const struct ali_mmc_host_ops ali_mmc_host_hw_v3_ops = {
	.set_clock	= ali_mmc_host_hw_v3_set_clock,
	.set_bus_width	= ali_mmc_host_hw_v3_set_bus_width,
	.set_timing	= ali_mmc_host_hw_v3_set_timing,
	.enable_force_clock = ali_mmc_host_hw_v3_enable_force_clock,
	.set_cmd	= ali_mmc_host_hw_v3_set_cmd,
	.start_cmd	= ali_mmc_host_hw_v3_start_cmd,
	.get_cmd_status	= ali_mmc_host_hw_v3_get_cmd_status,
	.get_response	= ali_mmc_host_hw_v3_get_response,
	.set_block	= ali_mmc_host_hw_v3_set_block,
	.set_dma	= ali_mmc_host_hw_v3_set_dma,
	.set_pio	= ali_mmc_host_hw_v3_set_pio,
	.pio_write	= ali_mmc_host_hw_v3_pio_write,
	.pio_read	= ali_mmc_host_hw_v3_pio_read,
	.pio_cleanup	= ali_mmc_host_hw_v3_pio_cleanup,
	.enable_irq	= ali_mmc_host_hw_v3_enable_irq,
	.enable_sdio_irq = ali_mmc_host_hw_v3_enable_sdio_irq,
	.get_and_clear_irq = ali_mmc_host_hw_v3_get_and_clear_irq,
};
