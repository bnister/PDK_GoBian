/*
 * Copyright (C) 2010, 2013-2015 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file ump_ukk_wrappers.c
 * Defines the wrapper functions which turn Linux IOCTL calls into _ukk_ calls for the reference implementation
 */


#include <asm/uaccess.h>             /* user space access */

#include "ump_osk.h"
#include "ump_uk_types.h"
#include "ump_ukk.h"
#include "ump_kernel_common.h"

/*20131227 yashi add*/
#ifdef ALI_UMP_MAPPING_KERNEL
#include "ump_kernel_interface_ref_drv.h"
#endif
/*
 * IOCTL operation; Allocate UMP memory
 */
#ifdef ALI_UMP_MAPPING_KERNEL
int ump_map_phy_wrapper(u32 __user * argument, struct ump_session_data  * session_data) 
{
	_ump_uk_map_phy_s user_interaction;
	//_mali_osk_errcode_t err;

	ump_dd_physical_block ump_memory_description;

	ump_dd_handle hMem;

	/*20131227 yashi add for ump test*/
	DBG_MSG(1, ("<ALi_KER_DEBUG>%s \n", __FUNCTION__));

	/* Sanity check input parameters */
	if (NULL == argument || NULL == session_data)
	{
		MSG_ERR(("NULL parameter in ump_ioctl_map_phy()\n"));
		return -ENOTTY;
	}

	/* Copy the user space memory to kernel space (so we safely can read it) */
	if (0 != copy_from_user(&user_interaction, argument, sizeof(user_interaction)))
	{
		MSG_ERR(("copy_from_user() in ump_ioctl_map_phy()\n"));
		return -EFAULT;
	}

	user_interaction.ctx = (void *) session_data;

	ump_memory_description.addr = user_interaction.phy_adr;
	ump_memory_description.size = user_interaction.size;


	/*20131227 yashi add for ump test*/
	DBG_MSG(1, ("<ALi_KER_DEBUG>argument = %x, addr = %x, size = %x\n", argument, ump_memory_description.addr, ump_memory_description.size));

	hMem = ump_dd_handle_create_from_phys_blocks( &ump_memory_description, 1);
	
	if( NULL == hMem)
		return  -EFAULT;

	user_interaction.secure_id = ump_dd_secure_id_get(hMem);

	user_interaction.ump_handle= hMem;

	user_interaction.ctx = NULL;

	if (0 != copy_to_user(argument, &user_interaction, sizeof(user_interaction)))
	{
		MSG_ERR(("copy_to_user() failed in ump_map_phy_wrapper()\n"));
		return  -EFAULT;
	}
	

	DBG_MSG(1, ("<ALi_KER_DEBUG>%s success, mem handle = %x, secure_id = %d\n", __FUNCTION__, ((_ump_uk_map_phy_s *)argument)->ump_handle, (u32)((_ump_uk_map_phy_s *)argument)->secure_id));

	return 0; /* success */
	
}
#endif
int ump_allocate_wrapper(u32 __user *argument, struct ump_session_data   *session_data)
{
	_ump_uk_allocate_s user_interaction;
	_mali_osk_errcode_t err;

	/* Sanity check input parameters */
	if (NULL == argument || NULL == session_data) {
		MSG_ERR(("NULL parameter in ump_ioctl_allocate()\n"));
		return -ENOTTY;
	}

	/* Copy the user space memory to kernel space (so we safely can read it) */
	if (0 != copy_from_user(&user_interaction, argument, sizeof(user_interaction))) {
		MSG_ERR(("copy_from_user() in ump_ioctl_allocate()\n"));
		return -EFAULT;
	}

	user_interaction.ctx = (void *) session_data;

	err = _ump_ukk_allocate(&user_interaction);
	if (_MALI_OSK_ERR_OK != err) {
		DBG_MSG(1, ("_ump_ukk_allocate() failed in ump_ioctl_allocate()\n"));
		return map_errcode(err);
	}
	user_interaction.ctx = NULL;

	if (0 != copy_to_user(argument, &user_interaction, sizeof(user_interaction))) {
		/* If the copy fails then we should release the memory. We can use the IOCTL release to accomplish this */
		_ump_uk_release_s release_args;

		MSG_ERR(("copy_to_user() failed in ump_ioctl_allocate()\n"));

		release_args.ctx = (void *) session_data;
		release_args.secure_id = user_interaction.secure_id;

		err = _ump_ukk_release(&release_args);
		if (_MALI_OSK_ERR_OK != err) {
			MSG_ERR(("_ump_ukk_release() also failed when trying to release newly allocated memory in ump_ioctl_allocate()\n"));
		}

		return -EFAULT;
	}

	return 0; /* success */
}
