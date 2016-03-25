/*
 * Copyright (C) 2012 Henrik Nordstrom <henrik@henriknordstrom.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <linux/module.h>
#include <linux/semaphore.h>
//#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/printk.h>

#ifndef CONFIG_FB_UMP_ALI
#error This file should not be built without UMP enabled
#endif
#include <linux/fb.h>
#include <ali_video_common.h>
#include "ali_fb.h"

static int _disp_get_ump_secure_id(struct fb_info *info, struct alifb_info *g_fbi,
				   unsigned long arg, int buf)
{
	u32 __user *psecureid = (u32 __user *) arg;
	ump_secure_id secure_id;

	if (!g_fbi->ump_wrapped_buffer[info->node][buf]) {
		ump_dd_physical_block ump_memory_description;

		ump_memory_description.addr = info->fix.smem_start;
		ump_memory_description.size = info->fix.smem_len;
		g_fbi->ump_wrapped_buffer[info->node][buf] =
			ump_dd_handle_create_from_phys_blocks
			(&ump_memory_description, 1);
	}
	secure_id = ump_dd_secure_id_get(g_fbi->ump_wrapped_buffer[info->node][buf]);
    printk("%s(), secure_id=%d\n", __FUNCTION__, secure_id);
	return put_user((unsigned int)secure_id, psecureid);
}

static int __init disp_ump_module_init(void)
{
	int ret = 0;

	disp_get_ump_secure_id = _disp_get_ump_secure_id;

	return ret;
}

static void __exit disp_ump_module_exit(void)
{
	disp_get_ump_secure_id = NULL;
}

module_init(disp_ump_module_init);
module_exit(disp_ump_module_exit);

MODULE_AUTHOR("ALiTech");
MODULE_DESCRIPTION("ALi FB MALI UMP module glue");
MODULE_LICENSE("GPL");
