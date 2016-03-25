/*
 * mmc-ali.h - ALi SDIO/SD/MMC driver header file
 *
 * Copyright (C) 2014-2015 ALi Corporation - http://www.alitech.com
 *
 * Authors: David.Shih  <david.shih@alitech.com>
 *	    Lucas.Lai	<lucas.lai@alitech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _MMC_ALI_H
#define _MMC_ALI_H

#define DRIVER_NAME "ali-mmc"

#define PIO_TRY_BOUND	8000

#define REG_V19_HC_VERSION	0x4C	/* HC Rev Reg for data sheet v1.9 */
#define REG_V18_HC_VERSION	0x48	/* HC Rev Reg for data sheet v1.8 */

#define CMD_STATUS_CRC7_ERR 	(1<<0)		/* CRC7 Error */
#define CMD_STATUS_CRC16_ERR 	(1<<1)		/* CRC16 Error */
#define CMD_STATUS_CMD_BUSY 	(1<<2)		/* Command Transfer Busy */
#define CMD_STATUS_DATA_BUSY 	(1<<3)		/* Data Transfer Busy */
#define CMD_STATUS_DATA_SERIN_0 (1<<4)		/* Serial Data 0 from SD Bus */
#define CMD_STATUS_DATA_SERIN_1 (1<<5)		/* Serial Data 1 from SD Bus */
#define CMD_STATUS_DATA_SERIN_2 (1<<6)		/* Serial Data 2 from SD Bus */
#define CMD_STATUS_DATA_SERIN_3 (1<<7)		/* Serial Data 3 from SD Bus */

#define INT_STATUS_CMD_END 	(1<<0)		/* Command Complete Interrupt */
#define INT_STATUS_DATA_END 	(1<<1)		/* Data Transfer Complete Interrupt */
#define INT_STATUS_SDIO 	(1<<2)		/* SDIO Interrupt */

#define ali_mmc_readb(host, reg)	(ioread8((void __iomem *)(host->base + reg)))
#define ali_mmc_readw(host, reg)	(ioread16((void __iomem *)(host->base + reg)))
#define ali_mmc_readl(host, reg)	(ioread32((void __iomem *)(host->base + reg)))
#define ali_mmc_writeb(host, val, reg)   (iowrite8(val, host->base + reg))
#define ali_mmc_writew(host, val, reg)   (iowrite16(val, host->base + reg))
#define ali_mmc_writel(host, val, reg)   (iowrite32(val, host->base + reg))

struct ali_mmc_platform_data {
	unsigned long    	max_frequency;  /* SD_IP_CLK */
	unsigned long		capability;
	unsigned long		cd_gpios;	/* Card detect GPIO */
	bool			cd_inverted;
	unsigned long		wp_gpios; 	/* Write protect GPIO */
	bool			wp_inverted;
	unsigned long		pin_group;
};

struct ali_mmc_host;

struct ali_mmc_host_ops {
	/* for ali_mmc_set_ios */
	void (*set_clock)(struct ali_mmc_host *host, unsigned int clock);
	void (*set_bus_width)(struct ali_mmc_host *host,
		unsigned char bus_width);
	void (*set_timing)(struct ali_mmc_host *host, unsigned char timing);

	/* for force cloeck enable/disable */
	void (*enable_force_clock)(struct ali_mmc_host *host, int enable);

	/* for cmd process */
	void (*set_cmd)(struct ali_mmc_host *host, struct mmc_command *cmd,
		struct mmc_data *data);
	void (*start_cmd)(struct ali_mmc_host *host);
	u32  (*get_cmd_status)(struct ali_mmc_host *host);
	void (*get_response)(struct ali_mmc_host *host,
		struct mmc_command *cmd);

	/* for data transfer */
	void (*set_block)(struct ali_mmc_host *host,
		unsigned int num_of_blocks, unsigned int block_length);
	void (*set_dma)(struct ali_mmc_host *host, dma_addr_t addr,
		unsigned int length, enum dma_data_direction direction);
	void (*set_pio)(struct ali_mmc_host *host, u32 direction);
	void (*pio_write)(struct ali_mmc_host *host, struct mmc_data *data);
	void (*pio_read)(struct ali_mmc_host *host, struct mmc_data *data);
	void (*pio_cleanup)(struct ali_mmc_host *host);

	/* Interrupt enable/disable */
	void (*enable_irq)(struct ali_mmc_host *host, struct mmc_data *data,
		int enable);
	void (*enable_sdio_irq)(struct ali_mmc_host *host, int enable);
	u32 (*get_and_clear_irq)(struct ali_mmc_host *host);
};

extern const struct ali_mmc_host_ops ali_mmc_host_hw_v4_ops;
extern const struct ali_mmc_host_ops ali_mmc_host_hw_v3_ops;

struct ali_mmc_host {
	struct resource		*res;
	void __iomem		*base;
	int			irq;
	struct ali_mmc_platform_data *pdata;
	const struct ali_mmc_host_ops *ops;

	struct mmc_host		*mmc;
	struct mmc_request	*req;
	struct mmc_command	*cmd;

	bool			use_pio;
	bool			sdio_irq_enable;
	bool			data_transferring;
	u32			cmd25_done_delay_ms;
	
	void __iomem		*virt_buf;
	dma_addr_t		phys_buf;

	unsigned int		data_size;
	enum dma_data_direction dma_dir;

	unsigned int		sg_len;

	u32			hc_rev_no;

	struct clk		*clk;

	spinlock_t		lock;

	unsigned int		clock;
	unsigned char		bus_width;
	unsigned char		timing;

	struct timer_list	watchdog;
	struct timer_list	sdio_timer;

	struct work_struct	cmdwork;
	struct work_struct	datawork;

};

#endif
