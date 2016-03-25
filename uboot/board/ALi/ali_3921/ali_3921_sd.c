#include <jffs2/load_kernel.h>
#include <common.h>
#include <asm/io.h>
#include <boot_common.h>
#include <asm/dma-mapping.h>
#include <linux/err.h>
#include <alidefinition/adf_boot.h>

#define u64 uint64_t
#define u32 uint32_t
#define u16 uint16_t
#define u8  uint8_t

#define MULTI_COMP_KERNEL_INDEX		0
#define MULTI_COMP_RAMDISK_INDEX	1
#define MULTI_COMP_SEE_INDEX		2
#define MULTI_COMP_AE_INDEX			3

#define BLOCK_SIZE 4096

static u_char *buf = NULL;

int load_part_sd_ext(const char *partName, u_char *LoadAddr, loff_t offset, size_t len)
{
	loff_t start;
	size_t size;
       size_t g_BlockSize = BLOCK_SIZE, realLen = 0;
	loff_t blockoffset, badblocksize = 0;
       int j = 0;
       size_t readsize = 0;
       

	printf("Load part sd ext '%s', offset:0x%llx, len:0x%x\n", partName, offset, len);

        printf("<%s>(%d): %s read addr: 0x%08x\n", __FUNCTION__, __LINE__, partName, LoadAddr);

	//if (get_part_by_name(partName, &start, &size) < 0)
       if((size = ali_get_filelength(partName)) < 0)
	{
		printf("<%s>(%d): '%s' is not exist\n", __FUNCTION__, __LINE__, partName);
		return -1;
	}

	if ((offset + len) > (loff_t)size)
	{
		printf("<%s>(%d): '%s' exceeds part size(offset=0x%llx len=0x%x partsize=0x%x)\n", __FUNCTION__, __LINE__, partName, offset, len, size);
		return -2;
	}

	/* Check LoadAddr 32 bytes aligned */
	if (((u32)LoadAddr & 0x1F) != 0)
	{
		printf("<%s>(%d): '%s' read data buffer(0x%08x) NOT 32 bytes aligned!\n", __FUNCTION__, __LINE__, partName, LoadAddr);
		return -3;
	}

       int cnt = 0;
        buf = (u_char*)malloc(g_BlockSize);
        if(buf == NULL)
        {
		printf("<%s>(%d): '%s' buffer malloc fail!\n", __FUNCTION__, __LINE__, partName);
		return -4;
	}
       if(len < g_BlockSize) {
            printf("<%s>(%d): '%s' buffer addr: 0x%08x!\n", __FUNCTION__, __LINE__, partName, buf);
            readsize = ali_read_file_at(partName, (int)offset, (unsigned char *)buf, (int)len);
            memcpy(LoadAddr, buf, len);
        }
       else {
        /*
            realLen = len;
            j = 0;
            while(realLen > g_BlockSize) {
            		readsize += ali_read_file_at(partName, offset+j*g_BlockSize, (unsigned char *)buf, g_BlockSize);
                    memcpy(LoadAddr+j*g_BlockSize, buf, g_BlockSize);
                    j++;
                    realLen -= g_BlockSize;
            }
    */
            readsize = ali_read_file_at(partName, (int)offset, (unsigned char *)LoadAddr, (int)len);
            //memcpy(LoadAddr+j*g_BlockSize, buf, realLen);
        }

        free(buf);
        buf = NULL;

        printf("<%s>(%d): '%s' read size: 0x%x\n", __FUNCTION__, __LINE__, partName, readsize);
        
	return readsize;
}
