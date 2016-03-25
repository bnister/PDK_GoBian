/*
 * mmc-ali-pinmux.c -MMC/SD/SDIO driver for ALi SoCs
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

#define M3733_MMC_RESET_BIT		30

void ali_mmc_soc_reset(void)
{
	void __iomem *soc_reset_reg = ioremap(0x18000080, 0x4);
	u32 reset_val = ioread32(soc_reset_reg);

	reset_val |= BIT(M3733_MMC_RESET_BIT);
	iowrite32(reset_val, soc_reset_reg);

	udelay(5);
	reset_val &= ~BIT(M3733_MMC_RESET_BIT);
	iowrite32(reset_val, soc_reset_reg);

	iounmap(soc_reset_reg);
}
