#
# Makefile for the ali shared memory communication driver
#
obj-y += mbx.o

mbx-y += shm_comm_hw.o ali_shm_platform.o

mbx-y += common.o compat_anon_inodes.o

mbx-y += mcomm_dev.o

#EXTRA_CFLAGS += -D__ALI_LINUX_KERNEL__ -D__MCAPI_TEST__ -I./ -I$(ALIDRIVERS_DIR)/libmcapi/
EXTRA_CFLAGS += -D__ALI_LINUX_KERNEL__  -D__BOOT_SEE_IN_UBOOT__  -I./ 

