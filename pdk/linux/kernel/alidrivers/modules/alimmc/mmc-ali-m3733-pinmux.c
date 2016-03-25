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

enum ali_3921_mmc_pin_group
{
	EMMC_BOOT = 0,   /* host switch to eMMC active */
	EMMC_SEL2,
	EMMC_SEL3,
};

#define SOC_PINMUX1_REG		0x18000088
#define SOC_PINMUX2_REG		0x1800008c
#define SOC_GPIO_ENABLE_REG	0x18000430	/* GPIO[31:0] enable register */
#define SOC_GPIOA_ENABLE_REG	0x18000434	/* GPIO[63:32] enable register */
#define SOC_GPIOB_ENABLE_REG	0x18000438	/* GPIO[95:64] enable register */
#define SOC_GPIOC_ENABLE_REG	0x1800043C	/* GPIO[127:96] enable register */

extern struct mutex ali_sto_mutex;

void ali_mmc_pinmux_restore(unsigned int mmc_group)
{
	void __iomem *pin_mux1_reg = ioremap(SOC_PINMUX1_REG, 0x4);
	void __iomem *pin_mux2_reg = ioremap(SOC_PINMUX2_REG, 0x4);
	/* read current register value */
	u32 PinMuxCtrl1 = ioread32(pin_mux1_reg);
	u32 PinMuxCtrl2 = ioread32(pin_mux2_reg);

	switch(mmc_group)
	{
	case EMMC_BOOT:
		PinMuxCtrl2 &= ~BIT(5);
		break;
	case EMMC_SEL2:
		PinMuxCtrl2 &= ~BIT(28);
		break;
	case EMMC_SEL3:
		PinMuxCtrl1 &= ~BIT(5);
		break;
	}

	iounmap(pin_mux1_reg);
	iounmap(pin_mux2_reg);

	
	if(mmc_group == EMMC_BOOT)
		mutex_unlock(&ali_sto_mutex);
}

void ali_mmc_pinmux_set(unsigned int mmc_group)
{
	void __iomem *pin_mux1_reg = ioremap(SOC_PINMUX1_REG, 0x4);
	void __iomem *pin_mux2_reg = ioremap(SOC_PINMUX2_REG, 0x4);
	void __iomem *gpio_reg = ioremap(SOC_GPIO_ENABLE_REG, 0x4);	/* GPIO[31:0] enable register */
	void __iomem *gpioa_reg = ioremap(SOC_GPIOA_ENABLE_REG, 0x4);	/* GPIO[63:32] enable register */
	void __iomem *gpiob_reg = ioremap(SOC_GPIOB_ENABLE_REG, 0x4);	/* GPIO[95:64] enable register */
	void __iomem *gpioc_reg = ioremap(SOC_GPIOC_ENABLE_REG, 0x4);	/* GPIO[127:96] enable register */

	/* read current register value */
	u32 PinMuxCtrl1 = ioread32(pin_mux1_reg);
	u32 PinMuxCtrl2 = ioread32(pin_mux2_reg);
	u32 gpio  = ioread32(gpio_reg);
	u32 gpioa = ioread32(gpioa_reg);
	u32 gpiob = ioread32(gpiob_reg);
	u32 gpioc = ioread32(gpioc_reg);

	if(mmc_group == EMMC_BOOT)
		mutex_lock(&ali_sto_mutex);

	switch(mmc_group)
	{
	case EMMC_BOOT:
		/* Disable GPIO Function */
		gpiob &= ~(BIT(94-64) | BIT(93-64));
		gpioc &= ~(BIT(99-96) | BIT(103-96) | BIT(104-96) | BIT(105-96) | BIT(101-96) | BIT(102-96) | BIT(100-96) | BIT(106-96) | BIT(107-96));

		/* Disable Share Function & Enable EMMC_BOOT_SEL */
		PinMuxCtrl1 &= ~(BIT(3) | BIT(19));
		PinMuxCtrl2 |= BIT(5);
		break;
	case EMMC_SEL2:
		/* Disable GPIO Function */
		gpioa &= ~(BIT(42-32) | BIT(37-32) | BIT(48-32) | BIT(49-32) | BIT(51-32) | BIT(46-32) | BIT(45-32) | BIT(43-32) | BIT(53-32) | BIT(35-32) | BIT(44-32));

		/* Disable Share Function & Enable EMMC_SEL2 */
		PinMuxCtrl1 &= ~(BIT(1) | BIT(5) | BIT(17) | BIT(18));
		PinMuxCtrl2 &= ~(BIT(1) | BIT(18) | BIT(20) | BIT(26));
		PinMuxCtrl2 |= BIT(28);
		break;
	case EMMC_SEL3:
		/* Disable GPIO Function */
		gpio  &= ~(BIT(11) | BIT(12));
		gpioa &= ~(BIT(42-32) | BIT(43-32) | BIT(44-32) | BIT(41-32));

		/* Disable Share Function & Enable EMMC_SEL3 */
		PinMuxCtrl1 &= ~(BIT(1) | BIT(17) | BIT(18) | BIT(23));
		PinMuxCtrl2 &= ~(BIT(1) | BIT(18) | BIT(20) | BIT(21) | BIT(22) | BIT(26) | BIT(28));
		PinMuxCtrl1 |= BIT(5);
		break;
	}

	iowrite32(gpio, gpio_reg);
	iowrite32(gpioa, gpioa_reg);
	iowrite32(gpiob, gpiob_reg);
	iowrite32(gpioc, gpioc_reg);
	iowrite32(PinMuxCtrl1, pin_mux1_reg);
	iowrite32(PinMuxCtrl2, pin_mux2_reg);

	iounmap(pin_mux1_reg);
	iounmap(pin_mux2_reg);
	iounmap(gpioa_reg);
	iounmap(gpiob_reg);
	iounmap(gpioc_reg);
}
