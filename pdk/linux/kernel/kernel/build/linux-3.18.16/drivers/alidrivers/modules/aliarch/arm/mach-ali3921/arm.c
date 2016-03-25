/*
 * Copyright (C) 2010, 2012-2014 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for:
 * - Realview Versatile platforms with ARM11 Mpcore and virtex 5.
 * - Versatile Express platforms with ARM Cortex-A9 and virtex 6.
 */
//#define FPGA_VERIFY
#ifndef FPGA_VERIFY 
//#include "/tpsa021/usrhome/tommy.chen/p4_zha1/PDK_Release_1.6.2_001_20140912/PDK1.6.2/linux/kernel/alidrivers/include/ali_board_config.h"
#include "ali_board_config.h"
#include <../arch/arm/mach-ali3921/include/mach/ali-s3921.h>
#endif
#include <asm/memory.h>
#include <asm/page.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/mali/mali_utgard.h>
#include <linux/mali/mali_kernel_common.h>
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>

#include "arm_core_scaling.h"
#include <linux/mali/mali_executor.h>

#define ALi_DEDICATED_START virt_to_phys(__G_ALI_MM_MALI_DEDICATED_MEM_START_ADDR)&0x7FFFFFFF

/*ALi's implementation of DFS*/

int ALi_set_freq(int setting_clock_step);
void ALi_report_clock_info(struct mali_gpu_clock **data);
int ALi_get_freq(void);

struct mali_gpu_clock *ALi_clock;

static int mali_core_scaling_enable = 0;

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data);
static u32 mali_read_phys(u32 phys_addr);
static void mali_write_phys(u32 phys_addr, u32 value);


#ifndef CONFIG_MALI_DT
static void mali_platform_device_release(struct device *device);


#if defined(CONFIG_ARM64)
/* Juno + Mali-450 MP6 in V7 FPGA */
static struct resource mali_gpu_resources_m450_mp6[] = {
	MALI_GPU_RESOURCES_MALI450_MP6_PMU(0x6F040000, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200)
};

#else
static struct resource mali_gpu_resources_m450_mp8[] = {
	MALI_GPU_RESOURCES_MALI450_MP8_PMU(0xFC040000, -1, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 68)
};

static struct resource mali_gpu_resources_m450_mp6[] = {
	MALI_GPU_RESOURCES_MALI450_MP6_PMU(0xFC040000, -1, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 70, 68)
};

static struct resource mali_gpu_resources_m450_mp4[] = {
	MALI_GPU_RESOURCES_MALI450_MP4_PMU(0xFC040000, -1, 70, 70, 70, 70, 70, 70, 70, 70, 70, 68)
};
#endif /* CONFIG_ARM64 */

/* for ALi implementation*/

static struct resource mali_gpu_resources_m300[] = {
	MALI_GPU_RESOURCES_MALI300_PMU(0xC0000000, -1, -1, -1, -1)
};

static struct resource mali_gpu_resources_m400_mp1[] = {
	MALI_GPU_RESOURCES_MALI400_MP1_PMU(0xC0000000, -1, -1, -1, -1)
};

static struct resource mali_gpu_resources_m400_mp2[] = {
	MALI_GPU_RESOURCES_MALI400_MP2_PMU(0xC0000000, -1, -1, -1, -1, -1, -1)
};
static struct resource mali_gpu_resources_m400_mp2_pmu[] = {
	MALI_GPU_RESOURCES_MALI400_MP2_PMU(0x19000000,110, 109, 107, 106, 105, 104)
};

#endif /*CONFIG_MALI_DT*/


static struct mali_gpu_device_data mali_gpu_data = {
#ifndef CONFIG_MALI_DT
	.pmu_switch_delay = 0xFF, /* do not have to be this high on FPGA, but it is good for testing to have a delay */
	.max_job_runtime = 60000, /* 60 seconds */
	.shared_mem_size = 256 * 1024 * 1024, /* 256MB */
#endif

/* ALi implementation*/
//	.dedicated_mem_start = 0x10708000, /* Physical start address (use 0xD0000000 for old indirect setup) */
//	.dedicated_mem_size = 0x4000000, /* 256MB */
//	.dedicated_mem_start = 0x00000000, /* Physical start address (use 0xD0000000 for old indirect setup) */
//	.dedicated_mem_size = 0x00000000, /* 256MB */


#if defined(CONFIG_ARM64)
	.fb_start = 0x5f000000,
	.fb_size = 0x91000000,
#else
/* ALi implementation*/
//	.fb_start = 0xe0000000,
//	.fb_size = 0x01000000,
	.fb_start = 0x10000000,
	.fb_size = 0x708000,
#endif
	.control_interval = 1000, /* 1000ms */
	.utilization_callback = mali_gpu_utilization_callback,
	.get_clock_info = ALi_report_clock_info,
	.get_freq = ALi_get_freq,
	.set_freq = ALi_set_freq,
};

#ifndef CONFIG_MALI_DT
static struct platform_device mali_gpu_device = {
	.name = MALI_GPU_NAME_UTGARD,
	.id = 0,
	.dev.release = mali_platform_device_release,
	.dev.dma_mask = &mali_gpu_device.dev.coherent_dma_mask,
	.dev.coherent_dma_mask = DMA_BIT_MASK(32),

	.dev.platform_data = &mali_gpu_data,
#if defined(CONFIG_ARM64)
	.dev.archdata.dma_ops = &noncoherent_swiotlb_dma_ops,
#endif
};

int mali_platform_device_register(void)
{
	int err = -1;
	int num_pp_cores = 0;
	u32 m400_gp_version;

MALI_DEBUG_PRINT(2, ("<ALi_DEBUG>,%s called\n", __FUNCTION__));
	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));

	/* Detect present Mali GPU and connect the correct resources to the device */

/*
#if defined(CONFIG_ARM64)
	if (mali_read_phys(0x6F000000) == 0x40601450) {
		MALI_DEBUG_PRINT(4, ("Registering Mali-450 MP6 device\n"));
		num_pp_cores = 6;
		mali_gpu_device.num_resources = ARRAY_SIZE(mali_gpu_resources_m450_mp6);
		mali_gpu_device.resource = mali_gpu_resources_m450_mp6;
	}
#else
	if (mali_read_phys(0xFC000000) == 0x00000450) {
		MALI_DEBUG_PRINT(4, ("Registering Mali-450 MP8 device\n"));
		num_pp_cores = 8;
		mali_gpu_device.num_resources = ARRAY_SIZE(mali_gpu_resources_m450_mp8);
		mali_gpu_device.resource = mali_gpu_resources_m450_mp8;
	} else if (mali_read_phys(0xFC000000) == 0x40600450) {
		MALI_DEBUG_PRINT(4, ("Registering Mali-450 MP6 device\n"));
		num_pp_cores = 6;
		mali_gpu_device.num_resources = ARRAY_SIZE(mali_gpu_resources_m450_mp6);
		mali_gpu_device.resource = mali_gpu_resources_m450_mp6;
	} else if (mali_read_phys(0xFC000000) == 0x40400450) {
		MALI_DEBUG_PRINT(4, ("Registering Mali-450 MP4 device\n"));
		num_pp_cores = 4;
		mali_gpu_device.num_resources = ARRAY_SIZE(mali_gpu_resources_m450_mp4);
		mali_gpu_device.resource = mali_gpu_resources_m450_mp4;
	}
#endif /* CONFIG_ARM64 */



	m400_gp_version = mali_read_phys(0x1900006C);
	if ((m400_gp_version & 0xFFFF0000) == 0x0C070000) {
		MALI_DEBUG_PRINT(4, ("Registering Mali-300 device\n"));
		num_pp_cores = 1;
		mali_gpu_device.num_resources = ARRAY_SIZE(mali_gpu_resources_m300);
		mali_gpu_device.resource = mali_gpu_resources_m300;
		mali_write_phys(0xC0010020, 0xA); /* Enable direct memory mapping for FPGA */
	} else if ((m400_gp_version & 0xFFFF0000) == 0x0B070000) {
#if 0
		u32 fpga_fw_version = mali_read_phys(0x19010000);
		if (fpga_fw_version == 0x130C008F || fpga_fw_version == 0x110C008F) {
			/* Mali-400 MP1 r1p0 or r1p1 */
			MALI_DEBUG_PRINT(4, ("Registering Mali-400 MP1 device\n"));
			num_pp_cores = 1;
			mali_gpu_device.num_resources = ARRAY_SIZE(mali_gpu_resources_m400_mp1);
			mali_gpu_device.resource = mali_gpu_resources_m400_mp1;
			mali_write_phys(0x19010020, 0xA); /* Enable direct memory mapping for FPGA */
		} else if (fpga_fw_version == 0x130C000F) {
			/* Mali-400 MP2 r1p1 */
			MALI_DEBUG_PRINT(4, ("Registering Mali-400 MP2 device\n"));
			num_pp_cores = 2;
			mali_gpu_device.num_resources = ARRAY_SIZE(mali_gpu_resources_m400_mp2);
			mali_gpu_device.resource = mali_gpu_resources_m400_mp2;
			mali_write_phys(0x19010020, 0xA); /* Enable direct memory mapping for FPGA */
		}
#else
			/* Mali-400 MP2 r1p1 */
			MALI_DEBUG_PRINT(4, ("Registering Mali-400 MP2 device\n"));
	#ifdef CONFIG_MALI400_MP1
			num_pp_cores = 1;
			err = platform_device_add_resources(&mali_gpu_device, mali_gpu_resources_m400_mp1_pmu, sizeof(mali_gpu_resources_m400_mp1_pmu) / sizeof(mali_gpu_resources_m400_mp2_pmu[0]));
	#else
			num_pp_cores = 2;
	err = 0;
			//err = platform_device_add_resources(&mali_gpu_device, mali_gpu_resources_m400_mp2_pmu, sizeof(mali_gpu_resources_m400_mp2_pmu) / sizeof(mali_gpu_resources_m400_mp2_pmu[0]));			
			mali_gpu_device.num_resources = ARRAY_SIZE(mali_gpu_resources_m400_mp2_pmu);
			mali_gpu_device.resource = mali_gpu_resources_m400_mp2_pmu;
	#endif						
			
			MALI_DEBUG_PRINT(2, ("<ALi_DEBUG>Mali-400 MP2 device %d core(s) Registered \n", num_pp_cores));

#endif /*if 0*/
	}else{
		MALI_DEBUG_PRINT(4, ("Unknown Mali version\n"));
	}

	if (0 == err) 
	{
#if 1 	
#ifndef FPGA_VERIFY	
	#if 0 // define for old version of snake
		//20131008 yashi add for dedicated memory
		mali_gpu_data.dedicated_mem_start = virt_to_phys(__G_MM_MALI_DEDICATED_MEM_START_ADDR)&0x7FFFFFFF;
		mali_gpu_data.dedicated_mem_size = __G_MM_MALI_DEDICATED_MEM_LEN;

		//20131008 yashi add for dedicated memory
		mali_gpu_data.fb_start = virt_to_phys(__G_MM_FB_START_ADDR)&0x7FFFFFFF;
		mali_gpu_data.fb_size = __G_MM_FB_SIZE;
	#else

		mali_gpu_data.shared_mem_size = totalram_pages << PAGE_SHIFT; /* get from linux kernel*/
		//mali_gpu_data.shared_mem_size = 128 * 1024 * 1024; /* get from linux kernel*/
printk("<ALi_DEBUG> shared_mem_size : %08x \n", mali_gpu_data.shared_mem_size);
		//20131016 yashi add for dedicated memory
		mali_gpu_data.dedicated_mem_start = virt_to_phys(__G_ALI_MM_MALI_DEDICATED_MEM_START_ADDR)&0x7FFFFFFF;
		//mali_gpu_data.dedicated_mem_size = __G_ALI_MM_MALI_DEDICATED_MEM_SIZE;
		mali_gpu_data.dedicated_mem_size = 0;
		
		//20131008 yashi add for dedicated memory
		mali_gpu_data.fb_start =  virt_to_phys(__G_ALI_MM_FB0_START_ADDR)&0x7FFFFFFF;
		mali_gpu_data.fb_size = __G_ALI_MM_FB0_SIZE;
printk("<ALi_DEBUG><%s>in %s:%4d , dedicated_mem_start = %x, dedicated_mem_size= %x, fb_start = %x, fb_size = %x \n", "KERNEL", 
	__func__, __LINE__, mali_gpu_data.dedicated_mem_start, mali_gpu_data.dedicated_mem_size, mali_gpu_data.fb_start, mali_gpu_data.fb_size);
#endif
#endif


printk("<Allen_debug> mali_gpu_device : %08x mali_gpu_data : %08x\n", &mali_gpu_device, &mali_gpu_data);
//err = platform_device_add_data(&mali_gpu_device, &mali_gpu_data, sizeof(mali_gpu_data));
#endif
		if (0 == err)
		{
			mali_gpu_device.dev.platform_data = &mali_gpu_data;
	/* Register the platform device */
	err = platform_device_register(&mali_gpu_device);
	if (0 == err) {
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
		pm_runtime_set_autosuspend_delay(&(mali_gpu_device.dev), 1000);
		pm_runtime_use_autosuspend(&(mali_gpu_device.dev));
#endif
		pm_runtime_enable(&(mali_gpu_device.dev));
#endif
		MALI_DEBUG_ASSERT(0 < num_pp_cores);
		mali_core_scaling_init(num_pp_cores);

		return 0;
	}
		}

		platform_device_unregister(&mali_gpu_device);
	}

	return err;
}

void mali_platform_device_unregister(void)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));

	mali_core_scaling_term();
	platform_device_unregister(&mali_gpu_device);

	platform_device_put(&mali_gpu_device);

	//mali_write_phys(0x19010020, 0x9); /* Restore default (legacy) memory mapping */

}

static void mali_platform_device_release(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}

#else /* CONFIG_MALI_DT */
int mali_platform_device_init(struct platform_device *device)
{
	int num_pp_cores;
	int err = -1;
	u32 m400_gp_version;


	/* Detect present Mali GPU and connect the correct resources to the device */
/*
#if defined(CONFIG_ARM64)
		if (mali_read_phys(0x6F000000) == 0x40601450) {
			MALI_DEBUG_PRINT(4, ("Registering Mali-450 MP6 device\n"));
			num_pp_cores = 6;
		}
#else
		if (mali_read_phys(0xFC000000) == 0x00000450) {
			MALI_DEBUG_PRINT(4, ("Registering Mali-450 MP8 device\n"));
			num_pp_cores = 8;
		} else if (mali_read_phys(0xFC000000) == 0x40400450) {
			MALI_DEBUG_PRINT(4, ("Registering Mali-450 MP4 device\n"));
			num_pp_cores = 4;
		}
#endif

*/

		m400_gp_version = mali_read_phys(0xC000006C);
		if ((m400_gp_version & 0xFFFF0000) == 0x0C070000) {
			MALI_DEBUG_PRINT(4, ("Registering Mali-300 device\n"));
			num_pp_cores = 1;
			mali_write_phys(0xC0010020, 0xA); /* Enable direct memory mapping for FPGA */
		} else if ((m400_gp_version & 0xFFFF0000) == 0x0B070000) {
			u32 fpga_fw_version = mali_read_phys(0xC0010000);
			if (fpga_fw_version == 0x130C008F || fpga_fw_version == 0x110C008F) {
				/* Mali-400 MP1 r1p0 or r1p1 */
				MALI_DEBUG_PRINT(4, ("Registering Mali-400 MP1 device\n"));
				num_pp_cores = 1;
				mali_write_phys(0xC0010020, 0xA); /* Enable direct memory mapping for FPGA */
			} else if (fpga_fw_version == 0x130C000F) {
				/* Mali-400 MP2 r1p1 */
				MALI_DEBUG_PRINT(4, ("Registering Mali-400 MP2 device\n"));
				num_pp_cores = 2;
				mali_write_phys(0xC0010020, 0xA); /* Enable direct memory mapping for FPGA */
			}
		}


	err = platform_device_add_data(device, &mali_gpu_data, sizeof(mali_gpu_data));

	if (0 == err) {
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
		pm_runtime_set_autosuspend_delay(&(device->dev), 1000);
		pm_runtime_use_autosuspend(&(device->dev));
#endif
		pm_runtime_enable(&(device->dev));
#endif
		MALI_DEBUG_ASSERT(0 < num_pp_cores);
		mali_core_scaling_init(num_pp_cores);
	}

	return err;
}

int mali_platform_device_deinit(struct platform_device *device)
{
	MALI_IGNORE(device);

	MALI_DEBUG_PRINT(4, ("mali_platform_device_deinit() called\n"));

	mali_core_scaling_term();

	mali_write_phys(0xC0010020, 0x9); /* Restore default (legacy) memory mapping */

	return 0;
}

#endif /* CONFIG_MALI_DT */

static u32 mali_read_phys(u32 phys_addr)
{
	u32 phys_addr_page = phys_addr & 0xFFFFE000;
	u32 phys_offset    = phys_addr & 0x00001FFF;
	u32 map_size       = phys_offset + sizeof(u32);
	u32 ret = 0xDEADBEEF;
	void *mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped) {
		ret = (u32)ioread32(((u8 *)mem_mapped) + phys_offset);
		iounmap(mem_mapped);
	}

	return ret;
}

static void mali_write_phys(u32 phys_addr, u32 value)
{
	u32 phys_addr_page = phys_addr & 0xFFFFE000;
	u32 phys_offset    = phys_addr & 0x00001FFF;
	u32 map_size       = phys_offset + sizeof(u32);
	void *mem_mapped = ioremap_nocache(phys_addr_page, map_size);
	if (NULL != mem_mapped) {
		iowrite32(value, ((u8 *)mem_mapped) + phys_offset);
		iounmap(mem_mapped);
	}
}


static int param_set_core_scaling(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);

	if (1 == mali_core_scaling_enable) {
		mali_core_scaling_sync(mali_executor_get_num_cores_enabled());
	}
	return ret;
}

static struct kernel_param_ops param_ops_core_scaling = {
	.set = param_set_core_scaling,
	.get = param_get_int,
};

module_param_cb(mali_core_scaling_enable, &param_ops_core_scaling, &mali_core_scaling_enable, 0644);
MODULE_PARM_DESC(mali_core_scaling_enable, "1 means to enable core scaling policy, 0 means to disable core scaling policy");

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data)
{
	if (1 == mali_core_scaling_enable) {
		mali_core_scaling_update(data);
	}
}

/*ALi's implementation of DFS*/
int ALi_set_freq(int setting_clock_step)
{
	int vol =  ALi_clock->item[setting_clock_step].vol;
	
	MALI_DEBUG_PRINT(2, ("<ALi_DBG>in %s, setting_clock_step = %d, set vol = %x\n", __FUNCTION__,setting_clock_step, vol));
	MALI_DEBUG_PRINT(2,("<ALi_DBG>clock gated(0x64) =%x\n",ALI_HWREG_GET_UINT32(0x64)));
	
	ALI_HWREG_SET_UINT32( ALI_HWREG_GET_UINT32(0x64)&(1<<16), 0x064 );
	ALI_HWREG_SET_UINT32((ALI_HWREG_GET_UINT32(0x7C)&0xffcfffff)|vol, 0x07C);    
	ALI_HWREG_SET_UINT32( ALI_HWREG_GET_UINT32(0x64)&(0<<16), 0x064 );
	
	return 1;	
}
#if 0

struct mali_gpu_clk_item {
	unsigned int clock; /* unit(MHz) */
	unsigned int vol;
};

struct mali_gpu_clock {
	struct mali_gpu_clk_item *item;
	unsigned int num_of_steps;
};
#endif
void ALi_report_clock_info(struct mali_gpu_clock **data)
{

	int i;
	
	struct mali_gpu_clk_item *item;
	
	if(NULL == *data)
	{
		*data = (struct mali_gpu_clock *)kmalloc(sizeof(struct mali_gpu_clock), GFP_KERNEL);
		ALi_clock = *data;
	}
	MALI_DEBUG_PRINT(2, ("<ALi_DBG>clock = %x, data =%x\n", ALi_clock, data));
	
	ALi_clock->num_of_steps = 3; 
	



	ALi_clock->item = (struct mali_gpu_clk_item *)kmalloc(sizeof(struct mali_gpu_clk_item) * ALi_clock->num_of_steps, GFP_KERNEL);


	item =  (struct mali_gpu_clk_item *)(ALi_clock->item);
	
	
	item[0].clock = 148;
	item[0].vol = 0;
	MALI_DEBUG_PRINT(2, ("<ALi_DBG>clock->item[0].clock = %d, clock->item[0].vol = %x\n", item[0].clock, item[0].vol));
	
	item[1].clock = 225;
	item[1].vol = 0x00100000;
	MALI_DEBUG_PRINT(2, ("<ALi_DBG>clock->item[1].clock = %d, clock->item[1].vol = %x\n", item[1].clock, item[1].vol)); 

	item[2].clock = 297;
	item[2].vol = 0x00200000;
	MALI_DEBUG_PRINT(2, ("<ALi_DBG>clock->item[2].clock = %d, clock->item[2].vol = %x\n", item[2].clock, item[2].vol)); 


}

int ALi_get_freq(void)
{
	unsigned int ret = ALI_HWREG_GET_UINT32(0x7C);
	ret = (ret>>20)&0x3;	//for clock bit = [20:21]
	MALI_DEBUG_PRINT(2, ("<ALi_DBG>in %s, current GPU clock step =%d, corresponding clock = %d\n", __FUNCTION__, ret, ALi_clock->item[ret].clock));
	return ret;
}

