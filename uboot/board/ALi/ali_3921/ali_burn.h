#ifndef __CONFIG_ALI_BURN_H
#define __CONFIG_ALI_BURN_H

#include <common.h>
#include "deviceinfo.h"
enum
{
	START_STATUS=0,
	SUCCESS_STATUS=1,
	WORK_STATUS=2,
	FAIL_STATUS=3
};

typedef unsigned char	BYTE;
typedef unsigned short	WORD;
typedef unsigned long	DWORD;
typedef	unsigned int	UINT;

#define MAX_PARTITION 128

struct file_info{
	unsigned char position_flag; /*存储位置标记，0:存储在sd/usb设备 1:存储在内存*/
	unsigned char Name[256];     /*文件存储路径*/
	unsigned int Addr;           /*文件存储的内存地址*/
	unsigned int Size;           /*文件尺寸*/
};
struct partition_info {
	unsigned int start;    /*烧写物理偏移地址*/
	unsigned int size;     /*分区尺寸，包含坏块*/
	
	unsigned char name[16]; /*分区名字*/
	unsigned int pos;     /*分区描述信息在ALi.ini文件中的存储偏移地址*/    
	unsigned int skip_flag; /*是否要烧录，skip_flag=1 该分区不需要被烧录*/
	struct file_info file; /*文件信息*/
};

/******************To Support Single File Burning*******************/

#define BUFREADCOMMENT	0x400
#define	COMMENT_LEN	 	256

#define CHECK_SIG(p, s)	\
	(((*(unsigned char *)(p)) == ((s) & 0xff)) && \
	((*((unsigned char *)(p) + 1)) == (((s) >> 8) & 0xff)) && \
	((*((unsigned char *)(p) + 2)) == (((s) >> 16) & 0xff)) && \
	((*((unsigned char *)(p) + 3)) == (((s) >> 24) & 0xff)))


#define READ_LONG(p)	\
	(DWORD)((*(unsigned char *)(p)) | \
	((*((unsigned char *)(p) + 1)) << 8) | \
	((*((unsigned char *)(p) + 2)) << 16 ) | \
	((*((unsigned char *)(p) + 3)) << 24 ))

#define READ_SHORT(p)	\
	(WORD)((*(unsigned char *)(p)) | \
	((*((unsigned char *)(p) + 1)) << 8))


typedef enum
{
    LocalFileHeaderSig = 0x04034b50,
    LocalFileDataDescriptorSig = 0x08074b50,
    CentralDirFileHeaderSig = 0x02014b50,
    CentralDirDigitalSig = 0x05054b50,
    EndOfCentralDirRecordSig = 0x06054b50,
}ZipSig;


typedef struct 
{
	unsigned long 	m_EndOfCentralDirRecordSig;		/* End of central directory signature =0x06054b50 */
	unsigned short 	m_NumberOfDisk;					/* Number of this disk */
	unsigned short 	m_DiskCentralDirStart;			/* Disk where central directory starts */
	unsigned short 	m_CentralDirRecordsNumber;		/* Number of central directory records on this disk */
	unsigned short 	m_CentralDirRecordsTotalNumber;	/* Total number of central directory records */
	unsigned long   m_CentralDirSize;				/* Size of central directory (bytes) */
	unsigned long 	m_CentralDirOffset;				/* Offset of start of central directory, relative to start of archive */
	unsigned char 	m_szComment[COMMENT_LEN];		/* Comment */
}EndOfCentralDirRecord;


typedef struct
{
	unsigned long 	m_LocalFileHeaderSig;
	unsigned short 	m_VersionNeeded;
	unsigned short 	m_Flag;
	unsigned short 	m_CompressionMethod;
	unsigned short 	m_Time;
	unsigned short 	m_Date;
	unsigned long 	m_Crc32;
	unsigned long	m_CompressedSize;
	unsigned long 	m_UncompressedSize;
	unsigned char 	m_szFileName[COMMENT_LEN];
	unsigned short 	m_ExtraFieldLength;
	unsigned char 	*m_ExtraField;

	unsigned long 	m_bIsDir;
	unsigned long 	m_DataOffset;
}LocalFileHeader;

typedef struct 
{
	unsigned long m_LocalFileDataDescriptorSig;
	unsigned long m_Crc32;
	unsigned long m_CompressedSize;
	unsigned long m_UncompressedSize;
}LocalFileDataDescriptor;


typedef struct 
{
	unsigned long 	m_CentralDirFileHeaderSig;
	unsigned short 	m_VersionMade;
	unsigned short 	m_VersionNeeded;
	unsigned short 	m_Flag;
	unsigned short 	m_CompressionMethod;
	unsigned short 	m_Time;
	unsigned short 	m_Date;
	unsigned long 	m_Crc32;
	unsigned long 	m_CompressedSize;
	unsigned long 	m_UncompressedSize;
	unsigned char 	m_szFileName[COMMENT_LEN];
	unsigned char 	m_szComment[COMMENT_LEN];		 /* Comment */
	unsigned short 	m_DiskNumberStart;
	unsigned short 	m_InternalFileAttributes;
	unsigned long  	m_ExternalFileAttributes;
	unsigned long  	m_LocalFileHeaderOffset;			/* Relative offset of local file header */
	unsigned short 	m_ExtraFieldLength;
	unsigned char 	*m_pExtraField;

	LocalFileHeader m_pLocalFileHeader;
    
}CentralDirFileHeader;



/******************To Support Single File Burning*******************/


typedef enum
{
 NAND_OTHERS=0,
 NAND_TOSHIBA,  // 1024+32
 NAND_HYNIX,      // 1024+46
 NAND_SAMSUNG,        // 512+16
 NAND_MICRON,
 NAND_RENESAS,     // 512
 NAND_ST,
 NAND_INTEL,    // 512 
 NAND_SANDISK
}E_NAND_VENDOR;



typedef enum
{
 ADDR_SB_CYC_3=0, //Small Block , address 3 cycle ( Col=1, Row=2 )
 ADDR_SB_CYC_4,  //Small Block ,address 4 cycle ( Col=1, Row=3 )
 ADDR_LB_CYC_4,  //large Block  , (Col=2, Row=2)
 ADDR_LB_CYC_5,  //large Block , (Col=2, Row=3)
 ADDR_LB_CYC_6  //large Block  , (Col=2, Row=4)
}NAND_ADDR_CYCLE;

typedef enum
{
 ECC_16BIT=0,  // 1024+32
 ECC_24BIT,      // 1024+46
 ECC_40BIT,      // 1024+74
 ECC_48BIT,      // 1024+46
 ECC_60BIT,      // 1024+46
 ECC_1BIT,        // 512+16
 ECC_CRC16,
 ECC_NONE      // 512
}NAND_ECC_MODE;

enum NAND_PAGE_TYPE_ 
{
	PAGE_512B = 0,
	PAGE_2K,
	PAGE_4K,
	PAGE_8K,
	PAGE_16K,
	PAGE_NONE
};

enum{
	ECC_16=0,
	ECC_24=1,	
	ECC_40=2,
	ECC_48=3,
	ECC_60=4,	
	ECC_1=5,
	CRC_MODE=3,
	EF_MODE=4,
	BA_MODE=5,
	ECC_24_SCRB=6,
};
/*
struct GW_CONFIGS
{
	unsigned long flag;//to check which item has configed by config.txt, one bit means one item.
	unsigned char tvSystem;
	unsigned char osd_ratio;
	unsigned char macStr[STB_FIRMINFO_MAC_LEN*4];
};*/

struct ST_NAND_DATA
{
	DWORD dwKeyID;
	DWORD dwBlocksPerChip;
	DWORD dwPagesPerBlock;
	DWORD dwSectorsPerPage;
	WORD wBusWidth;
	BYTE bRowCycle;		//add
	BYTE bBanks;
	BYTE bReadCycle;
	BYTE bWriteCycle;
	BYTE bPage2BlockAddrShift;	
	BYTE fProgramCache; //new
	char NandId[20];
	char NandName[48];
	NAND_ECC_MODE bEccMode;	
	UINT uiSkipRowAddrShift;  //new
	E_NAND_VENDOR eVendorType; //new
	BYTE fReliableMode; //new
	BYTE bRev;
	WORD wAliId;
};

void ali_set_single_file_status( int value );
int ali_get_single_file_status( void );

void trim(char *s);
char *ali_itoa(int n, char *s, int radix);
int ali_tolower(char ch);
unsigned long ali_strtoul(const char *cp,char **endp,unsigned int base);
int GetIniKeyString(char *title,char *key,unsigned char *p,int length,char *str);
int ali_dev_fat_register(int dev,int devicetype);
int ali_get_filelength(char *filename);
int ali_is_access_directory(char *directory);
int ali_is_access_file(char *filename);
int ali_read_file(char *filename,unsigned char *p);
int ali_read_zip_file(char *filename, char *zipfilename, unsigned char *p);
int ali_read_file_at(char *filename,int pos,unsigned char *p,int count);
int ali_get_nandinfo(char *nandfile,char nand_ids[10]);
int ali_set_config();
void ali_set_led_status(int status);
int ali_parse_ini(char *inifile);
int ali_burn_process();

int ali_get_central_dir_record(char *cf,EndOfCentralDirRecord *pRecord);
int ali_get_central_dir_file_header(unsigned char *pBuf,CentralDirFileHeader *pHeader, unsigned long nLength );
int ali_get_local_file_header(unsigned char *pBuf, LocalFileHeader *pHeader);
int ali_parse_zip( char *szFile );
int ali_get_local_file(char *ini_name, unsigned long *pOffset, unsigned long *pLength);


#endif
