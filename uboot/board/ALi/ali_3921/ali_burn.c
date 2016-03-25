#include <common.h>
#include <asm/io.h>
#include <boot_common.h>
#include <asm/dma-mapping.h>
#include <linux/err.h>

#include <nand.h>
#include <mmc.h>
#include "ali_burn.h"
#include <alidefinition/adf_boot.h>

#define DEBUG_ALI_BURN
#define READ_BACK_CHECK
#if defined(DEBUG_ALI_BURN)
#define aliburn_printf printf
#else
#define aliburn_printf(args...) do{}while(0)
#endif

//#define DEBUG_ALI_BURN_TEST
#if defined(DEBUG_ALI_BURN_TEST)
#define aliburn_test_printf printf
#else
#define aliburn_test_printf(args...) do{}while(0)
#endif

#define DEBUG_ALI_BURN_CONSUMP
#if defined(DEBUG_ALI_BURN_CONSUMP)
#define aliburn_consump_printf printf
#else
#define aliburn_consump_printf(args...) do{}while(0)
#endif

#ifdef DEBUG_ALI_BURN_CONSUMP
static unsigned long  r_tt_p = 0;
static unsigned long  r_tt_rd = 0;
static unsigned long  r_tt_wr = 0;
#endif

#define TOLOWER(x) ((x)+32)
#define isxdigit(c) (('0'<=(c)&&(c)<='9') ||('a'<=(c)&&(c)<='f')||('A'<=(c)&&(c)<='F'))

#define SUPPORT_SINGLE_FILE

//#define S3921_PMI
#define C3921_BOOTAREA

#define	MAX_INI_BUF_LEN			(1024*1024*2)

struct partition_info g_partition_info[MAX_PARTITION];
unsigned long g_part_num = 0;
static unsigned char filebuffer[MAX_INI_BUF_LEN]__attribute__((aligned(4096)));
static unsigned char pmi_NandInfo[64];
static unsigned long g_total_burn_size =0;
static unsigned long g_burn_size =0;


static int	single_file_mode = 0; 
static char zip_file_name[256]="";
EndOfCentralDirRecord m_CentralDirRecord;
CentralDirFileHeader m_pCentralDirFileHeader[MAX_PARTITION];
unsigned int m_DirFileCount = 0;



#ifdef S3921_PMI
unsigned int g_LoaderStart[4];
unsigned int g_LoaderLength;
unsigned char g_PMIbf[1024*1024*1];
unsigned int g_dram_address;
unsigned int g_DramInitStartLBA, g_DramInitLength;
unsigned char g_DRAM_Config[2048];	
unsigned int g_pmiLength;
char g_loaderfile[256]="";
#endif

#ifdef C3921_BOOTAREA
unsigned int g_LoaderStart[4];
unsigned int g_LoaderLength;
unsigned int g_dram_address;
unsigned int g_DramInitStartLBA, g_DramInitLength;
unsigned char g_DRAM_Config[2048];	
unsigned int g_pmiLength;
char g_loaderfile[256]="";
#endif

#define BURN_BLOCK_LEN (1024*1024*16)      
static unsigned char burn_bl_block_buffer[BURN_BLOCK_LEN] __attribute__((aligned(4096)));
static unsigned char burn_block_buffer[BURN_BLOCK_LEN] __attribute__((aligned(4096)));
static unsigned char cmp_burn_block_buffer[BURN_BLOCK_LEN] __attribute__((aligned(4096)));

//#define SD_PINMUX_SWITCH

nand_info_t *g_nand;
struct ST_NAND_DATA g_nand_info;
unsigned int	g_PageSize;
unsigned int	g_BlockSize;
unsigned int	g_EccUnitPerBlock;	
unsigned int	g_PMIBlockNo[4];
unsigned int	g_EccSize;	
unsigned int	g_LbaUnit;
unsigned int	g_RowCycle;
unsigned int	g_BlocksPerChip;
unsigned char g_bReadCycle;
unsigned char g_bWriteCycle;
unsigned char g_bEccMode;
char g_ali_NandName[48]="";
unsigned char g_deviceid;
char g_curDir[100]="/burn/";

//struct GW_CONFIGS gw_config;

static void ltrim(char *s)
{
	char *p;
	p=s;
	while(*p==' ' || *p=='\t'){*p++;}
	strcpy(s,p);
}

static void rtrim(char *s)
{
	int i;
	i =strlen(s)-1;
	while((s[i]==' ' || s[i]=='\t')&&i>=0){i--;};
	s[i+1]='\0';
}

void trim(char *s)
{
	ltrim(s);
	rtrim(s);
}

static char lower[] = "0123456789abcdefghijklmnopqrstuvwxyz";
static char * strrev(char *s)
{
    int j,k,c;
    for(k=0;s[k] != 0;k++);
    for(j=0,k--;j<k;j++,k--) {
	c = s[j];
	s[j] = s[k];
	s[k] = c;
    }
    return s;
}
char *ali_itoa(int n, char *s, int radix)
{
    unsigned int uval;
    int i, sign;

    if((sign = (n < 0)) && radix == 10)
	uval = -n;
    else
	uval = n;

    i=0;
    do {
      s[i++] = lower[uval % radix];
      uval /= radix;
    }while(uval>0);

    if (sign)
	s[i++] = '-';
    s[i] = '\0';
    return strrev(s);
}

int ali_tolower(char ch)

{

   if(ch >= 'A' && ch <= 'Z')

      return (ch + 0x20);

   return ch;

}
#if 1
static int  ali_strncmp(const char *s1,const char *s2,int len)
{
	while(len--){
		//printf("s1 0x%02x,s2 0x%02x,len %d\n",*s1,*s2,len);
			if(*s1==0 || *s1!=*s2)
			{
				//printf("ret %d\n",*s1-*s2);
				return *s1-*s2;
			}	
			s1++;
			s2++;
		}
	//printf("ret1\n");
	return 0;
}
#endif

void ali_set_single_file_status( int value )
{
	single_file_mode = value;
}

int ali_get_single_file_status( void )
{
	return single_file_mode;
}



#if 0
/*********************************************************************
* Function: Get the position of the [PARTITION(number)] in the ALi.ini
**********************************************************************/
int GetIniKeyPos( char *title, unsigned char *p,int length )
{	
	unsigned char rtnval;  
	unsigned char sz_line[256] = " "; 
	unsigned char tmp_str[256]= " ";    
	int i = 0;
	int count = 0, ret = 0;
	int pos = -1;
	
	memset(sz_line,0,256);
	memset(tmp_str,0,256);

	if((p == NULL)||(length==0))
	{
		aliburn_printf("have no such file\n");
		return -1;
	}
	
	while(length-count)
	{
		rtnval = *(p+count);      
		count++;
		
		sz_line[i++] = rtnval;
		
		if(rtnval == '\n')
		{
			sz_line[--i]='\0';
			i=0;

			strcpy(tmp_str,"[");
			strcat(tmp_str,title);
			strcat(tmp_str,"]");

			ret = ali_strncmp(tmp_str,sz_line,strlen(tmp_str));
			if(ret == 0)
			{
				//find the title
				pos = count;
				aliburn_printf("----find title[%s],pos[%d]\n",title, count);
                break;
			}				
		}
	}

EXIT:

	return pos;
}
#endif

static char szLine[256]; 
static char tmpstr[256];
int GetIniKeyString(char *title,char *key,unsigned char *p,int length,char *str)
{
	//FILE *fp;	
	unsigned char rtnval;
	int i = 0, j = 0;
	int flag= 0;
	char *tmp;
	int count = 0;
	int ret1 =0;
	int ret_code = 1;
	
	memset(szLine,0,256);
	memset(tmpstr,0,256);
//	printf("%s %s\n",title,key);

	if(str == NULL)
	{
		aliburn_printf("parameter error **\n");
		return -1;
	}
	if((p ==NULL)||(length==0))
	{
		aliburn_printf("have no such file\n");
		return -1;
	}
	
	while(length-count)
	//while(!feof(fp))
	{
		rtnval = *(p+count);
		count++;
		
		szLine[i++] = rtnval;
		
		if(rtnval == '\n')
		{
			szLine[--i]='\0';
			i=0;
			tmp = strchr(szLine, '=');

			//printf("tmp %s,str %s\n",tmp,szLine);			
			if((tmp!=NULL)&&(flag==1))
			{
				
				if(strstr(szLine,key)!=NULL)
				{
					if('#'==szLine[0])
					{
					
					}
					else if('\/' ==szLine[0]&& '\/'== szLine[1])
					{
					
					}
					else
					{
						strcpy(tmpstr,tmp+1);
						trim(tmpstr);
						//fclose(fp);
						//aliburn_printf("key: %s string:%s\n",key,tmpstr);
						if(strlen(tmpstr)>0)
						{
							strcpy(str,tmpstr);
							ret_code =0;
//							aliburn_printf("find, title[%s],key[%s]\n",title,key);
							goto EXIT;
						}
						else
						{
							ret_code=2;
							goto EXIT;
						}		
					}
				}
			}
			else
			{
				strcpy(tmpstr,"[");
				strcat(tmpstr,title);
				strcat(tmpstr,"]");
				//printf("tmpstr %s,szLine %s,len %d\n",tmpstr,szLine,strlen(tmpstr));
				//ret =strncmp(tmpstr,szLine,strlen(tmpstr));
				ret1 =ali_strncmp(tmpstr,szLine,strlen(tmpstr));
				if(ret1==0)
				{
					//ÕÒµ½title
					flag=1;
//					aliburn_printf("find, title[%s]\n",title);
				}				
			}
		}
	}
	//fclose(fp);
EXIT:

//	if(ret_code > 0)
//		printf("ret_code %d,flag %d\n",ret_code,flag);
    
	return ret_code;
}

static int ali_atoi(const char * str)
{
	const char *pstr = str;
	int result = 0;
	int sign = 1;

	//found sign and skip blank
	while(*pstr==' ')
		pstr++;
	if(*pstr == '-')
	{
		sign = -1;
		pstr++;
	}
	if(*pstr == '+')
	{
		pstr++;
	}
	
	while ((unsigned int)(*pstr - '0')<10u)
	{
		result = result * 10 + *pstr - '0';
		pstr++;
	}

	result= result * sign;

	return result;
}
int atoi(const char * str) __attribute__((alias("ali_atoi")));

static unsigned long ali_atoi16(const char  *str)
{
	unsigned char  *tmp = (unsigned char *)str;
	unsigned long sum = 0;
	unsigned char data = 0;

	while (*tmp != '\0')
	{
		switch (*tmp)
		{
		case 'A':
		case 'a':
			data = 10;
			break;
		case 'B':
		case 'b':
			data = 11;
			break;
		case 'C':
		case 'c':
			data = 12;
			break;
		case 'D':
		case 'd':
			data = 13;
			break;
		case 'E':
		case 'e':
			data = 14;
			break;
		case 'F':
		case 'f':
			data = 15;
			break;
		default:
			data = *tmp - '0';
			break;
		}
		sum = sum * 16 + data;
		tmp++;
	}
	return sum;
}

unsigned long ali_strtoul(const char *cp,char **endp,unsigned int base)  
{  
	unsigned long result = 0;
    
	if (!base){
			base = 10;
			if (*cp == '0'){
				base=8;
			cp++;

			if ((ali_tolower(*cp)=='x') && (isxdigit(cp[1])) ){
				cp++;
				base=16;
			}
		}
	} else if (base==16){
		if (cp[0] == '0' && ali_tolower(cp[1]) == 'x')
		cp += 2;
	}

	//printf("base %d cp %s\n",base,cp);
	if(base==10)
		result =ali_atoi(cp);
	else if(base==8)
		aliburn_printf("not suppot\n");
	else if(base==16)
		result = ali_atoi16(cp);
	
	if (endp)
		endp = (char *)cp;
	return result;
}

static void MakeValue(unsigned int nn, unsigned char *bf)
{
	unsigned int i, j;

	j = 0;
	for (i=0;i<4;i++)
	{
		bf[j] = (unsigned char)((nn >> (i<<3)) & 0xFF);
		j ++;
	}
}
/*
 * Register FAT File System
 * Return 0 on success, 1 otherwise.
 */
 #if defined(CONFIG_ALI_MMC)
#if defined(CONFIG_ALI_SD)
int ali_dev_fat_register(int dev,int devicetype)
{
	//int dev;
	int part;
	char *ep;
	block_dev_desc_t *dev_desc=NULL;
	int ret =0;
	
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_set( __LINE__ );
#endif	

	dev =0;
	part =1;
	dev = (int)simple_strtoul("0", &ep, 16);
	if(devicetype==1)
		dev_desc = get_dev("mmc",dev);
	else if(devicetype==2)
		dev_desc = get_dev("usb",dev);
	else{
		aliburn_printf("devicetype invalid **\n");
		ret = 1;
		goto EXIT;
	}	
	if (dev_desc == NULL) {
		puts("\n** Invalid boot device **\n");
		ret = 1;
		goto EXIT;

	}
	if (*ep) {
		if (*ep != ':') {
			puts("\n** Invalid boot device, use `dev[:part]' **\n");
			ret = 1;
			goto EXIT;
		}
		part = (int)simple_strtoul(++ep, NULL, 16);
	}
	if (fat_register_device(dev_desc,part)!=0) {
		if(devicetype==1)
		{
			aliburn_printf("\n** Unable to use %s %d:%d for fatls **\n",
				"mmc", dev, part);
		}	
		else if(devicetype==2)
		{
			aliburn_printf("\n** Unable to use %s %d:%d for fatls **\n",
				"usb", dev, part);
		}
		ret  = 1;
		goto EXIT;
	}	
EXIT:
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_release( __LINE__ );
#endif			
	return ret;
}

int ali_get_filelength(char *filename)
{
    unsigned long offset = 0;  
	unsigned char buf[10];
	int 	size = 0, count = 0;     
	int 	ret =0;
    
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_set( __LINE__ );
#endif	

	if(filename==NULL)
	{
		aliburn_printf("filename invalid **\n");
		ret = -1;
		goto EXIT;
	}


	count =0;
	//buf is not used
	size = file_fat_read_filelength(filename, buf, count);

	if(size>=0){
	    ret = size;
	    goto EXIT;
	}
	else{
	    aliburn_printf("%s(),file %s not exist\n",__FUNCTION__, filename);
	    ret = -1;
	    goto EXIT;
	}   
  
EXIT:
    
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_release( __LINE__ );
#endif	

	return ret;
}	

int ali_get_zip_filelength(char *filename)
{
    unsigned long offset = 0;  
	unsigned char buf[10];
	int 	size = 0, count = 0;     
	int 	ret =0;
    
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_set( __LINE__ );
#endif	

	if(filename==NULL)
	{
		aliburn_printf("filename invalid **\n");
		ret = -1;
		goto EXIT;
	}


	ret = ali_get_local_file(filename, &offset, &size);
	if( ret < 0 )
	{
	    aliburn_printf(" %s(), L[%d],Can't find file! FileName[%s] \n", __FUNCTION__,__LINE__,filename);  
	    goto EXIT;
	}
	else
	{
		ret = size;			
	}  

EXIT:
    
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_release( __LINE__ );
#endif	

	return ret;
}


int ali_is_access_directory(char *directory)
{
	int ret;
	int ret_code=0;
	ret =0;
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_set( __LINE__ );
#endif		
	ret = file_fat_ls(directory);
	
	if(ret!=0)
	{
		aliburn_printf("directory %s not exist **\n",directory);
		ret_code = 0;
		goto EXIT;
	}
	else
	{
		ret_code = 1;
		goto EXIT;
	}
EXIT:
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_release( __LINE__ );
#endif	
	return ret_code;
}

int ali_is_access_file(char *filename)
{
	int size;
	unsigned char *p;
	int count;
	unsigned char buf[10];
	int ret =0;
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_set( __LINE__ );
#endif			

	if(filename==NULL)
	{
		aliburn_printf("filename invalid **\n");
		ret =0;
		goto EXIT;	
	}
	
	//offset=0x84000000;
	p = buf;
	count =0;
	size = file_fat_read_filelength(filename, (unsigned char *)p, count);
	//printf("file %s filelength %d\n",filename,size);	
	if(size<0){
		aliburn_printf("file no exist\n");
		ret = 0;
		goto EXIT;
	}else{
		ret = 1;
		goto EXIT;
	}		

EXIT:
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_release( __LINE__ );
#endif	


	return ret;
}	

int ali_read_file(char *filename,unsigned char *p)
{
	int size;
	int count;
	int ret =0;

#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_set( __LINE__ );
#endif	

	if(filename==NULL)
	{
		aliburn_printf("filename[%s] invalid **\n", filename);
		ret = -1;
		goto EXIT;
	}
	
	if(p==NULL)
	{
		aliburn_printf("read address invalid **\n");
		ret = -1;
		goto EXIT;
	}

	count =0;
	size = file_fat_read(filename, (unsigned char *)p, count);
	printf("ali_read_file  size=%d\n",size);
	//aliburn_printf("file %s filelength %d\n",filename,size);  
	if(size<0)
	{
	    aliburn_printf("%s(),file not exist\n",__FUNCTION__);
		ret = -1;
		goto EXIT;
	}	
	else
	{
		ret = size;
	}

EXIT:
    
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_release( __LINE__ );
#endif	

	return ret;		
}

/**************************************************************************
* Function: Read the whole file in the ZIP package
***************************************************************************/
int ali_read_zip_file(char *filename, char *zipfilename, unsigned char *p)
{
	unsigned int offset = 0,size = 0;
	int count;
	int ret =0;

#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_set( __LINE__ );
#endif	

	if( (filename == NULL) || (zipfilename == NULL) )
	{
		aliburn_printf("filename invalid **\n");
		ret = -1;
		goto EXIT;
	}
	
	if(NULL == p)
	{
		aliburn_printf("read address invalid **\n");
		ret = -1;
		goto EXIT;
	}


	//work in the single file update mode
	
	ret = ali_get_local_file(filename, &offset, &size);
	if( ret < 0 )
	{  
        aliburn_printf(" %s(), L[%d],Can't find file! FileName[%s] \n", __FUNCTION__,__LINE__,filename); 
	    goto EXIT;
	}
    
	ret = file_fat_read_at(zipfilename, offset,(unsigned char *)p, size);	
	//aliburn_printf("file %s filelength %d\n",filename,size);	
	if( ret <0)
	{
		aliburn_printf("file[%s] not exist in file[%s], ret[%d] \n",filename,zipfilename,ret);
		ret = -1;
		goto EXIT;
	}	
	else
	{
		ret = size;
		goto EXIT;
	}	     

EXIT:
    
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_release( __LINE__ );
#endif	

	return ret;
		
}


int ali_read_file_at(char *filename,int pos,unsigned char *p,int count)
{
	int size;
	int ret =0;
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_set( __LINE__ );  
#endif	

	if(filename==NULL)
	{
		printf("filename invalid **\n");
		ret = -1;
		goto EXIT;
	}

    printf("<%s>(%d): file name: %s, pos: %d, read addr: 0x%08x, count: %d\n", __FUNCTION__, __LINE__, filename, pos, p, count);
	
	if(p==NULL)
	{
		printf("read address invalid **\n");
		ret = -1;
		goto EXIT;
	}
	
	size = file_fat_read_at(filename, pos,(unsigned char *)p, count);
	
	printf("file %s filelength %d\n",filename,size);	
	if(size < 0)
	{
		printf("%s(),file not exist\n",__FUNCTION__);
		ret =-1;
		goto EXIT;
	}	
	else
	{
		ret = size;
		goto EXIT;
	}	

EXIT:
		
#ifdef SD_PINMUX_SWITCH
	ali_sd_pinmux_release( __LINE__ );
#endif

	return ret;
}

/*
 * get nand infomaiton,by searching the NandList File
 * Return 0 on success, -1 or 1 otherwise.
 */
int ali_get_nandinfo(char *nandfile,char nand_ids[10])
{
        int size;
        struct ST_NAND_DATA nand_info;
        unsigned char *p = NULL;
        unsigned char file_buffer[MAX_INI_BUF_LEN];
        int find =0;
        char str[21];

	p = file_buffer;
    
	size = ali_read_file(nandfile,file_buffer);
	if(size<0)
	{
		aliburn_printf("load nandfile error **\n");
		return -1;	
	}
		
	
	str[20]='\0';
	while(size)
	{	
		if(size<sizeof(struct ST_NAND_DATA))
			break;
		memcpy(&nand_info,p,sizeof(struct ST_NAND_DATA));
		
		if(nand_info.wAliId !=0x0139)
			break;
		
		p = p + sizeof(struct ST_NAND_DATA);
		
		size = size - sizeof(struct ST_NAND_DATA);
		memcpy(str,nand_info.NandId,20);
		//aliburn_printf("str %s\n",str);
		if(memcmp(nand_info.NandId,nand_ids,10)==0)
		{
			memcpy(&g_nand_info,&nand_info,sizeof(struct ST_NAND_DATA));
			find =1;
			aliburn_printf("PagesPerBlock=%d SectorsPerPage=%d\n",g_nand_info.dwPagesPerBlock,g_nand_info.dwSectorsPerPage);
			break;
		}		
	}

	if(find==1)
		return 0;
	else if(find==0)
		return 1;
}

/*Set global config variable of Nand Flash and PMI*/
int ali_set_config()
{
	int	Page2BlkShift;
	//Bytes Per ECC Sector
	if ((g_nand_info.bEccMode == ECC_NONE) || (g_nand_info.bEccMode==ECC_1BIT))
	{
		g_EccSize = 512;	
		g_LbaUnit = 512;
	}
	else
	{
		g_EccSize = 1024;
		g_LbaUnit = 1024;
	}	

	g_PageSize = g_nand_info.dwSectorsPerPage << 9;
	g_BlockSize = g_nand_info.dwPagesPerBlock*g_PageSize;
	g_EccUnitPerBlock = g_BlockSize / g_EccSize;
	g_BlocksPerChip =g_nand_info.dwBlocksPerChip;
	g_bReadCycle = g_nand_info.bReadCycle;
	g_bWriteCycle = g_nand_info.bWriteCycle;
	strcpy(g_ali_NandName,g_nand_info.NandName);
	g_bEccMode = g_nand_info.bEccMode;
	
	if (g_nand_info.dwPagesPerBlock==32)				
		Page2BlkShift = 5;				
	else if (g_nand_info.dwPagesPerBlock==64)				
		Page2BlkShift = 6;
	else if (g_nand_info.dwPagesPerBlock==128)
		Page2BlkShift = 7;
	else if (g_nand_info.dwPagesPerBlock==192)
		Page2BlkShift = 8;
	else if (g_nand_info.dwPagesPerBlock==256)
		Page2BlkShift = 8;
						
	g_PMIBlockNo[0] = 0 >> Page2BlkShift;		
	g_PMIBlockNo[1] = 256 >> Page2BlkShift;
	g_PMIBlockNo[2] = 512 >> Page2BlkShift;		
	g_PMIBlockNo[3] = 768 >> Page2BlkShift;	

	return 0;
}	
#define MAX_CONFIGTXT_BUF_LEN 1024*1024



int GetConfigTxtString(char *key,unsigned char *p,int length,char *str)
{
	//FILE *fp;	
	unsigned char rtnval;
	int i = 0, j = 0;
	//int flag= 0;
	char *tmp;
	int count = 0;
	int ret1 =0;
	int ret_code = 1;
	static char txtLine[256]; 
	static char txttmpstr[256];	
	
	memset(txtLine,0,256);
	memset(txttmpstr,0,256);
	//printf("GetConfigTxtString  key=  %s \n",key);

	if(str == NULL)
	{
		aliburn_printf("parameter error **\n");
		return -1;
	}
	if((p ==NULL)||(length==0))
	{
		aliburn_printf("have no such file\n");
		return -1;
	}
	ret_code=-2;
	while(length-count)
	//while(!feof(fp))
	{
		rtnval = *(p+count);
		count++;
		
		txtLine[i++] = rtnval;
		
		if(rtnval == '\n'||(length==count))
		{
			if(length!=count)
				txtLine[--i]='\0';
			else
				txtLine[i]='\0';
			i=0;
			//printf("txtLine= '%s'\n",txtLine);	
			trim(txtLine);
			if('#'==txtLine[0])
			{
				//aliburn_printf("Discription:   %s \n",txtLine);
			}
			else 
			{
				tmp = strchr(txtLine, '=');
				//printf("tmp %s,str %s\n",tmp,txtLine);			
				if(tmp!=NULL)
				{
					if(strstr(txtLine,key)!=NULL)
					{			
						strcpy(txttmpstr,tmp+1);
						trim(txttmpstr);
						//fclose(fp);
						aliburn_printf("key: %s string:%s\n",key,txttmpstr);
						if(strlen(txttmpstr)>0)
						{
							strcpy(str,txttmpstr);
							ret_code =0;
							aliburn_printf("find, key[%s]\n",key);
							goto EXIT;
						}
						else
						{
							ret_code=-2;
							goto EXIT;
						}		
					}
				}
			}
		
		}
	}
EXIT:
	return ret_code;
}

#include <stdarg.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ctype.h>
long go_simple_strtoul(const char *cp,char **endp,unsigned int base)
{
	long result = 0;
	long value;
	int flag=-1;

	if (*cp == '0' && *(cp+1)!='\n' && *(cp+1)!='\0'  && *(cp+1)!='\r' ) {
		cp++;
		if ((*cp == 'x') && isxdigit(cp[1])) {
			base = 16;
			cp++;
		}
		if (!base) {
			base = 8;
		}
	}
	if (!base) {
		base = 10;
	}
	while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
	    ? toupper(*cp) : *cp)-'A'+10) < base) {
		result = result*base + value;
		cp++;
		flag=0;
	}
	if (endp)
		*endp = (char *)cp;
	
	if(flag==-1)
		return -1;
	
	return result;
}
static int mac_str_invalid(char * mac_str)
{
	int ret=0;
	char macstr[20],tmpmacstr[20];
	//char * tmpstr;
	char str[5];
	memset(str,0,sizeof(str));
	str[0]=":";
	memcpy(macstr,mac_str,sizeof(macstr));
	trim(mac_str);
	int i=0,val;
	for(i=0;i<6;i++)
	{
		memset(tmpmacstr,0,sizeof(tmpmacstr));
		memcpy(tmpmacstr,&macstr[i*3],3);
		//printf("mac_str_invalid  %d\n",memcmp(&tmpmacstr[2],str,1));
		if(i!=5&&(tmpmacstr[2]!=':'))
		{
			printf("mac_str_invalid   %s need to be \":\"  \n",&tmpmacstr[2]);
			return -1;
		}
		if(!isxdigit(tmpmacstr[0])&&!isdigit(tmpmacstr[0]))
		{
			printf("mac_str_invalid  %s not valid\n",&tmpmacstr[0]);
			return -1;
		}
		if(!isxdigit(tmpmacstr[1])&&!isdigit(tmpmacstr[1]))
		{
			printf("mac_str_invalid  %s not valid\n",&tmpmacstr[1]);
			return -1;
		}
		if(i==5&&((tmpmacstr[2]!='\0')&&(tmpmacstr[2]!='\n')))
		{
			printf("mac_str_invalid  (%s) mac string too long\n",&tmpmacstr[2]);
			return -1;
		}
	}

	return ret;
}
static int macstr_to_hex(char *string, unsigned char *cbuf)  
{  
	BYTE high, low;  
	int idx=0;
	int ii=0;  
	while(ii<6)
	{
		
		if(string[idx]==':'){
			idx++;
			if(idx>32)
				break;
			continue;
		}
		high = string[idx];  
		low = string[idx+1];  
		if(high>='0' && high<='9')  
		    high = high-'0';  
		else if(high>='A' && high<='F')  
		    high = high - 'A' + 10;  
		else if(high>='a' && high<='f')  
		    high = high - 'a' + 10;  
		else  
		    return -1;  
		  
		if(low>='0' && low<='9')  
		    low = low-'0';  
		else if(low>='A' && low<='F')  
		    low = low - 'A' + 10;  
		else if(low>='a' && low<='f')  
		    low = low - 'a' + 10;  
		else  
		    return -1;  
		
		idx=idx+2;
		
		cbuf[ii++] = high<<4 | low;  
	}  
	return 0;  
}  
int ali_parse_configtxt(char *inifile,ADF_BOOT_BOARD_INFO *boardinfo )
{
	//unsigned char *p = filebuffer;
	char ini_name[32] = "config.txt";
	char keystr[256];
	char config_str[MAX_CONFIGTXT_BUF_LEN];
	char tmp;
	long tmp_l;
	unsigned long start = 0, offset = 0;
	unsigned int  size = 0 , par_size = 0;
	int 	i = 0,j = 0;    
	int 	file_length = 0, position = 0;
	int 	ret = 0;
	aliburn_printf("ali_parse_configtxt  %s\n",inifile);
	memset(config_str, 0, sizeof(MAX_CONFIGTXT_BUF_LEN));
	if(inifile==NULL)
		ret=ali_read_file(ini_name, config_str);
	else
		ret=ali_read_file(inifile, config_str);
	if(ret<0)
		return ret;
	memset(keystr, 0,sizeof(keystr));
	if(GetConfigTxtString("tv_mode",config_str,ret,keystr)==0)
	{
		tmp_l=go_simple_strtoul(keystr,NULL,10);
		if(tmp_l<0)
			printf("config.txt:  hdmi_mode=%s set fail\n",keystr);
		else{
			
			if(tmp_l<=20)
			{
				boardinfo->avinfo.tvSystem=(char)tmp_l;
				aliburn_printf("ali_parse_configtxt hdmi_mode(config.txt)=%s  tvSystem=%d \n",keystr,boardinfo->avinfo.tvSystem);
			}else
				printf("ali_parse_configtxt hdmi_mode(config.txt)=%s not support\n",keystr);
		}
	}
	memset(keystr, 0,sizeof(keystr));
	if(GetConfigTxtString("osd_ratio",config_str,ret,keystr)==0)
	{
		tmp_l=go_simple_strtoul(keystr,NULL,10);
		if(tmp_l<0)
			printf("config.txt:  osd_ratio=%s set fail\n",keystr);
		else{
			if(tmp_l>=80&&tmp_l<=100)
			{
				boardinfo->avinfo.osd_ratio=(char)tmp_l;
				aliburn_printf("ali_parse_configtxt osd_ratio(config.txt)=%s  osd_ratio=%d \n",keystr,boardinfo->avinfo.osd_ratio);
			}else
				printf("ali_parse_configtxt osd_ratio(config.txt)=%s  <80 or >100  not set to boardinfo\n",keystr,boardinfo->avinfo.osd_ratio);
		}
	}
	memset(keystr, 0,sizeof(keystr));
	if(GetConfigTxtString("mac",config_str,ret,keystr)==0)
	{
		if(mac_str_invalid(keystr)>=0)
		{
			macstr_to_hex(keystr,boardinfo->macinfo.phyaddr1);
			aliburn_printf("ali_parse_configtxt mac(config.txt)=%s \n",keystr);
			setenv("ethaddr", keystr);	
		}
	}
	memset(keystr, 0,sizeof(keystr));
	if(GetConfigTxtString("framebuffer_ignore_alpha",config_str,ret,keystr)==0)
	{
		tmp_l=go_simple_strtoul(keystr,NULL,10);
		if(tmp_l<0)
			printf("config.txt:  framebuffer_ignore_alpha=%s set fail\n",keystr);
		else{
			if(tmp_l==1||tmp_l==0)
			{
				boardinfo->avinfo.framebuffer_ignore_alpha=(char)tmp_l;
				aliburn_printf("ali_parse_configtxt framebuffer_ignore_alpha(config.txt)=%s  framebuffer_ignore_alpha=%d \n",keystr,boardinfo->avinfo.framebuffer_ignore_alpha);
			}else
				printf("ali_parse_configtxt framebuffer_ignore_alpha(config.txt)=%s  should be 0 or 1\n",keystr);
		}
	}
	return ret;
}

/******************************************************************************************
* Function: Parse ALI.ini file to create the partion_info list
* 		1) ALI.ini is in the U-Disk or SD Card
*		2) ALI.ini is packed in the ZIP file
* Input Parameter:
* 	*szfile : The fine name string pointer
* Output:
* 	Return 0 success,-1 otherwise
******************************************************************************************/
int ali_parse_ini(char *inifile)
{
    unsigned char *p = filebuffer;
    char partition_name[32] = "PARTITION";
    char ini_name[32] = "ALI.ini";
    char str_num[10]="";
    char Temp[32];
    char tmpfile[128]="";  
    unsigned long start = 0, offset = 0;
    unsigned int  size = 0 , par_size = 0;
    int 	i = 0,j = 0;    
    int 	file_length = 0, position = 0;
    int 	ret = 0;

    // Parse the INI file 
    for (i = 0; i < MAX_PARTITION; i++){
        g_partition_info[i].size = 0;	
        g_partition_info[i].start = 0;
        g_partition_info[i].file.Size = 0;
        strcpy((char *)(g_partition_info[i].file.Name),"");
        g_partition_info[i].pos = 0;
        g_partition_info[i].skip_flag = 0;
    }	

    memset((void *)filebuffer, 0x00, MAX_INI_BUF_LEN); 

    if(1 == ali_get_single_file_status()){
        //Need furthur survey for the delay	
        aliburn_printf(" %s(),L[%d],inifile[%s],size[%d], To analize the ZIP packet \n",__FUNCTION__,__LINE__,inifile, size);

        //work in the single file update mode
        //Parse ZIP package to get the all the file info
        ret = ali_parse_zip( inifile );
        if( ret < 0 ){
            aliburn_printf(" Parse zip package fail!! \n"); 
            goto EXIT;
        } 

//        printf("==11= %s(),L[%d],ali_parse_zip(),inifile[%s] \n",__FUNCTION__,__LINE__,inifile );

        strcpy((char *)zip_file_name, inifile);

        size = ali_read_zip_file(ini_name, inifile, filebuffer );
        if( size < 0 ){
            aliburn_printf(" Fail to read ALI.ini! FileName[%s],File[%s],ret[%d] \n",\
                        	ini_name, inifile, ret);
            ret = -1;            
            goto EXIT;
        }           

    //        printf("==11= %s(),L[%d],inifile[%s],size[%d], \n",__FUNCTION__,__LINE__,inifile, size);
    }else{
        aliburn_printf("%s(),inifile[%s],size[%d] \n",__FUNCTION__,inifile, size);
        size = ali_read_file(inifile,filebuffer);
        if(size < 0){
            aliburn_printf("load %s error **\n",inifile);
            ret = -1;
            goto EXIT;
        }		
    }            

    aliburn_printf("%s(),inifile[%s],size[%d] \n",__FUNCTION__,inifile, size);
    //find loader segment
    memset(Temp,0,32);
    ret = GetIniKeyString("STARTUP-FILE", "LOADER", p, size, Temp);
    if(0 == ret){
        if(1 == ali_get_single_file_status()){
            strcpy(g_loaderfile,Temp);
            g_LoaderLength = ali_get_zip_filelength( g_loaderfile );
        }else{
            strcpy(tmpfile,g_curDir);
//      strcat(tmpfile,"/");
            strcat(tmpfile,Temp);
            strcpy(g_loaderfile,tmpfile);   

            g_LoaderLength = ali_get_filelength( g_loaderfile );
        }            
    	            
    	if( g_LoaderLength < 0 ){
            aliburn_printf("get_loader_length fail **\n");
            ret =-1;
            goto EXIT;
    	}
        
#ifdef C3921_BOOTAREA        
        strcpy((char *)(g_partition_info[0].file.Name), g_loaderfile);
        g_partition_info[0].file.Size = g_LoaderLength;	
        g_partition_info[0].start = 0;
#endif

    }else{
        aliburn_printf("didn't find loader , size[%d], ret[%d]**\n", size,ret);
    }

#ifdef S3921_PMI

    //find start address segment
    if(0==GetIniKeyString("SYSTEM-START-ADDRESS", "START_ADDR",p,size,Temp))
    	g_dram_address = ali_strtoul(Temp, NULL, 0);		
    else{
    	strcpy(Temp,"2148532224");
    	g_dram_address=ali_strtoul(Temp, NULL, 0);
    }

    //find ddr abs segment
    if(0==GetIniKeyString("STARTUP-FILE", "DRAM",p,size,Temp)){
    	if(1 == ali_get_single_file_status()){
    		ret = ali_read_zip_file(Temp, inifile, g_DRAM_Config );
    		if( ret < 0 ){
    		    aliburn_printf(" Fail to read file[*s] ! FileName[%s],File[%s],ret[%d] \n",\
    		                	Temp, inifile, ret);           
    		    goto EXIT;
    		}        
    	}else{
            strcpy(tmpfile,g_curDir);
            //strcat(tmpfile,"/");
            strcat(tmpfile,Temp);

            if(ali_read_file(tmpfile, g_DRAM_Config)<0){
                aliburn_printf("load dram file fail **\n");
                ret = -1;
                goto EXIT;
            }        
    	}
    }else{
    	aliburn_printf("get dram file fail **\n");
    }	
#endif    
    //#endif

    ret = GetIniKeyString("PARTITION-COUNT","COUNT",p,size,Temp);
    if(0 == ret ){
    	g_part_num = ali_strtoul(Temp , NULL, 0);
    	g_part_num++;
    	aliburn_printf("part num %ld \n",g_part_num);	
    }else{
    	aliburn_printf("can't find part count **, size[%d], ret[%d] \n",size, ret);
    	ret = -1;
    	goto EXIT;
    }

#if 1	
    g_partition_info[0].start = 0;
    if(0==GetIniKeyString("ALI-PRIVATE-PARTITION0", "SIZE",p,size,Temp)){
        g_partition_info[0].size = ali_strtoul(Temp, NULL, 0);
        g_partition_info[0].skip_flag = 0;
    }	
#endif

#if 0
    //For debug
    //Parse all the position of the Partition[*] Info in the ALi.ini file
    for(i = 1; i < g_part_num; i++)
    {
    	strcpy(partition_name,"PARTITION");
    	ali_itoa(i,str_num,10);
    	strcat(partition_name,str_num);

    	position =  GetIniKeyPos( partition_name, p,size );     
        if( position < 0 )
    	{
    		aliburn_printf("Not found Partition[%d]\n", i);
            
    		ret = -1;
    		goto EXIT;						
        } 
        g_partition_info[i].pos = position;
        
        aliburn_printf("Partition[%d], pos[%d]\n", i, position);
    }     
#endif

    par_size = size;
    memset(Temp,0,32);	
    for(i = 1; i < g_part_num; i++)
    {
    	strcpy(partition_name,"PARTITION");
    	ali_itoa(i,str_num,10);
    	strcat(partition_name,str_num);

#if 0
        // The data length of the description info about Partition[i]
        if( i == (g_part_num -1 ) )
    		size = par_size ;
        else
    		size = g_partition_info[i+1].pos ;          
#endif
        	
        //0 Partition start addr
        g_partition_info[i].start = g_partition_info[i-1].start + g_partition_info[i-1].size;
        for (j = 0; j < 16; j++){
            Temp[j] = 0;
            g_partition_info[i].name[j] = 0;
        }
        
        if( 0 == GetIniKeyString(partition_name,"NAME",p,size,Temp) )
            strcpy(g_partition_info[i].name,Temp);	
        
        // 1 Partition Image File Length
        g_partition_info[i].file.Size = 0;
    	
        if(0==GetIniKeyString(partition_name,"FILE",p,size,Temp)){
            if(1 == ali_get_single_file_status())
                strcpy(g_partition_info[i].file.Name,Temp);         
            else{
                strcpy(g_partition_info[i].file.Name,g_curDir);
                //strcat(g_partition_info[i].file.Name,"/");
                strcat(g_partition_info[i].file.Name,Temp); 
            }                		
    	}else
    		goto NoFileName;	

        if(1 == ali_get_single_file_status())
            file_length = ali_get_zip_filelength(g_partition_info[i].file.Name);
        else
            file_length = ali_get_filelength(g_partition_info[i].file.Name);
            
    	if(file_length != -1){
            g_partition_info[i].file.Size = file_length;
            //			aliburn_printf("g_partition_info[%d].file.Size %d\n",i,file_length);		
    	}else{
            aliburn_printf("didn't get file length**\n");
    	}

#if 0		
    	if (g_partition_info[i].file.Size & (g_BlockSize -1))
    	{
    		//g_partition_info[i].file.Size = (g_partition_info[i].file.Size / g_BlockSize +1) * g_BlockSize;
    		g_partition_info[i].file.Size = g_partition_info[i].file.Size;
    	}	
#endif


NoFileName:		

    	// 2  Partition reserve SIZE
    	g_partition_info[i].size = 0;  	
    	if(0 == GetIniKeyString(partition_name,"SIZE", p, size, Temp)){
            g_partition_info[i].size = ali_strtoul(Temp, NULL, 0);
//			aliburn_printf("g_partition_info[%d].size 0x%08x\n",i,g_partition_info[i].size);
//			aliburn_printf("g_BlockSize 0x%x\n",g_BlockSize);
            if (g_partition_info[i].size & (g_BlockSize -1))
                g_partition_info[i].size = (g_partition_info[i].size / g_BlockSize + 1) * g_BlockSize;
            	
//			aliburn_printf("g_partition_info[%d].size 0x%08x \n\n",i,g_partition_info[i].size);	
    	}else{
            aliburn_printf("p(%d) Not found SIZE= !?\n", i);
            ret =-1;
            goto EXIT;
    	}

    	//To parse the partition skip flag
        if( g_partition_info[i].file.Size > 0 )	{
            if(0 == GetIniKeyString(partition_name,"SKIP", p, size, Temp)){
                g_partition_info[i].skip_flag = ali_strtoul(Temp, NULL, 0);
            //			    aliburn_printf("g_partition_info[%d].skip_flag[0x%08x],Temp[%s]\n",i,g_partition_info[i].skip_flag,Temp); 
            }     
        }          
    }
    aliburn_printf("partion	      start		size		Name		FileName		FileSize	skip_flag\n");
    for(i=0;i<g_part_num;i++){
    	aliburn_printf("%d	0x%08x	0x%08x	%16s	%32s	0x%08x	%d\n",\
    	            i,g_partition_info[i].start,g_partition_info[i].size,g_partition_info[i].name,g_partition_info[i].file.Name,g_partition_info[i].file.Size,g_partition_info[i].skip_flag);
    }		

EXIT:
    ali_set_led_status(WORK_STATUS);
    return ret;
}

#endif
#endif

static int nand_erase_partition(nand_info_t *info, loff_t off, size_t size)
{
	int 	i = 0;
	int 	ret = 0, result = 0 ;
    unsigned int    start, len;

	start = off;
	len = size;

	result = 0;

	for(i = 0; i < (size / g_BlockSize); i++)
	{   
	    start = off + i * g_BlockSize;
	    
	    if ( nand_block_isbad(info, start ) )
	    {
	        continue;
	    }

	    //Erase the block       
	    len = g_BlockSize;    

	    ret = nand_erase(info, start, len);
	    if( ret < 0)
	    {
	        result = ret;       
	        aliburn_printf(" %s(), L[%d], nand_erase fail, start[0x%08x], len[0x%08x], ret[%d] \n",__FUNCTION__,__LINE__,start, len,ret);    

            //Need to udpate the Bad_Block_Table again
            //nand_update_bbt();
	    }                    
	}

ERASE_EXIT:
    
    return result;

}


#ifdef S3921_PMI
void prepare_s3921_pmi_data(unsigned char *pmi_buf)
{
	int i;
	unsigned char *nand_info;

	nand_info=pmi_NandInfo;

	//Blocks Per Chip
	MakeValue((unsigned int)g_nand_info.dwBlocksPerChip, nand_info);
				
	//Bytes Per Page
	MakeValue(((unsigned int)g_nand_info.dwSectorsPerPage) * 512, nand_info + 4);
				
	//Pages Per Block
	MakeValue((unsigned int)g_nand_info.dwPagesPerBlock, nand_info + 8);

	MakeValue(g_EccSize, nand_info + 12);
				
	//Bytes Per LBA
	MakeValue(512, nand_info + 16);

	//Bytes Redundent
	if (g_nand_info.bEccMode == ECC_16BIT)
		MakeValue(32, nand_info+20);
	else if (g_nand_info.bEccMode == ECC_24BIT)
		MakeValue(46, nand_info+20);
	else if (g_nand_info.bEccMode == ECC_40BIT)
		MakeValue(74, nand_info+20);
	else if (g_nand_info.bEccMode == ECC_48BIT)
		MakeValue(88, nand_info+20);
	else if (g_nand_info.bEccMode == ECC_60BIT)
		MakeValue(110, nand_info+20);
	else if (g_nand_info.bEccMode == ECC_1BIT)
		MakeValue(16, nand_info+20);
	else
		MakeValue(2, nand_info+20);

	//Bytes Per Row
	if (g_nand_info.bEccMode == ECC_1BIT)
		g_nand_info.bRowCycle = 2;
	MakeValue((unsigned int)g_nand_info.bRowCycle, nand_info+24);
				
	//ECC type
	MakeValue((unsigned int)g_nand_info.bEccMode, nand_info+28);

	//Logical End
	MakeValue(0, nand_info+32);
				
	//Read Clock
	MakeValue((unsigned int)g_nand_info.bReadCycle, nand_info+36);
	//hWnd->MakeValue(0x33, hWnd->m_NandInfo+36);

	//Write Clock
	MakeValue((unsigned int)g_nand_info.bWriteCycle, nand_info+40);
	//hWnd->MakeValue(0x33, hWnd->m_NandInfo+40);
				
	//Physical PM block				
	MakeValue(0, nand_info+44);

	//Shift Address to Block
	if (g_nand_info.dwPagesPerBlock == 64)
		MakeValue(6, nand_info + 48);
	else if (g_nand_info.dwPagesPerBlock == 128)
		MakeValue(7, nand_info + 48);
	else
		MakeValue(8, nand_info + 48);
				
	//Program cache
	MakeValue(0, nand_info + 52);
				
	//Skip Row Address Shift
	MakeValue(0, nand_info + 56);
	
	g_pmiLength = 4096+4096;
	
	for (i=0; i<4; i ++) // miniboot, 8192 bytes offset in block
	{
		//fix loader start address @ page pmiLength 0f each PMI start //2013/07/09 david
		g_LoaderStart[i] = (g_PMIBlockNo[i] * g_BlockSize) + g_pmiLength ;	
	}
	g_LoaderLength = ((g_LoaderLength + 1023) / 1024) * 1024;

	//g_partition_info[0].size += ( (g_LoaderLength / g_BlockSize) + 1) * g_BlockSize;	

	memset(pmi_buf, 0xAA, g_PageSize * 16);
				
	//PMI initial value is 0
	memset(pmi_buf, 0x00, g_PageSize * 4);

	//PMI byte 0~11 ECC Test Area
	switch (g_nand_info.bEccMode)
	{
		case ECC_60BIT:
                    for (i=0; i<4; i++)
                        *(pmi_buf + i) = (char) 0xFF;
                    for (i=8; i<12; i++)
                        *(pmi_buf + i) = (char) 0xFF;
                    break;		

		case ECC_48BIT:
			for (i=6; i<12; i++)
				*(pmi_buf + i) = (char) 0xFF;
			break;

		case ECC_40BIT:
			for (i=0; i<6; i++)
				*(pmi_buf + i) = (char) 0xFF;
			break;

		case ECC_24BIT:	
			for (i=0; i<12; i++)
				*(pmi_buf + i) = (char) 0xFF;
			break;

		case ECC_16BIT:
			for (i=0; i<12; i++)
				*(pmi_buf + i) = (char) 0x00;
			break;

		case ECC_1BIT:						
			for (i=4; i<8; i++)
				*(pmi_buf + i) = (char) 0xFF;
			break;

		default:	//no ECC					
			break;
	}

				
	//PMI byte 12~15 Signature
	*(pmi_buf + 12) = 0x39;
	*(pmi_buf + 13) = 0x21;
						
	*(pmi_buf + 14) = 0x55;
	*(pmi_buf + 15) = 0xAA;
				
	//PMI byte 16~75 NAND Config
	for (i=0;i<60;i++)
		*(pmi_buf + i + 16) = pmi_NandInfo[i];
				
	*(pmi_buf + 16 + 48) = 0x0;

	g_DramInitStartLBA = 2048 / g_LbaUnit;	/*fix address and length*/ 
	g_DramInitLength = 2048 / g_LbaUnit;
	//PMI byte 76~83 Dram initial code, start and length
	//start @ 2048 byte, length 2048
	MakeValue(g_DramInitStartLBA, pmi_buf + 76);	
	MakeValue(g_DramInitLength, pmi_buf + 80);	

	memcpy(pmi_buf + 2048, g_DRAM_Config,  2048);
	//memcpy(pmi_buf + 4096, hWnd->m_BadBlockTable,  4096);

	MakeValue(4096 / g_LbaUnit, pmi_buf + 84);	
	MakeValue(4096 / g_LbaUnit, pmi_buf + 88);	

	//PMI byte 92~99 Security Data
	//start @ 0, length 0, no use for 36XX
	MakeValue(0, pmi_buf + 92);	
	MakeValue(0, pmi_buf + 96);	

	//PMI byte 100~107 Loader 
	//start @ 0, length 0, no use for 36XX

	//PMI byte 108~115 Resource
	//start @ 0, length 0, no use for 36XX
	MakeValue(0, pmi_buf + 108);	
	MakeValue(0, pmi_buf + 112);	
				
	//PMI byte 116~123 Linux Kernel 
	//start @ 0, length 0, no use for 36XX
	MakeValue((unsigned int) (g_partition_info[1].start / g_LbaUnit), pmi_buf + 116);	
	MakeValue((unsigned int) (g_partition_info[1].size / g_LbaUnit), pmi_buf + 120);

	//PMI byte 132 Kernel start dram address 
	MakeValue(g_dram_address, pmi_buf + 132);

	//PMI byte 256 Parition Number, max 31 parititions
	MakeValue(g_part_num, pmi_buf + 256);
				
	//PMI byte 260~512 m_NandInfo Parition, max 31 parititions
	for(i=0; i<g_part_num; i++)
	{ 
		MakeValue((unsigned int) (g_partition_info[i].start / g_EccSize), pmi_buf + 260 + i * 8);
		MakeValue((unsigned int) (g_partition_info[i].size / g_EccSize), pmi_buf + 264 + i * 8);

		/* Can ignore. just check flag for PMI*/
		//if (FTL_Partition == i)
		//	hWnd->MakeValue((unsigned int) (hWnd->m_PttInfo[i].size / hWnd->m_EccSize) | (1<<30), hWnd->m_PMIbf + 264 + i * 8);
		//if (UBI_Partition == i)
		//	hWnd->MakeValue((unsigned int) (hWnd->m_PttInfo[i].size / hWnd->m_EccSize) | (1<<31), hWnd->m_PMIbf + 264 + i * 8);

		//#ifdef  USE_FTL
		//	*(hWnd->m_PMIbf + 264 + 8 * i + 3) = *(hWnd->m_PMIbf + 264 + 8 * i + 3) | (FTLsig[i]<<6); 
		//#endif					
					
					//copy partition name
		memcpy(&pmi_buf[512 + i * 16], &g_partition_info[i].name, 16);
	}
	unsigned int index1, index2, index3, index4;
	unsigned int index5, index6;
					
	for(i=0; i<g_part_num; i++)
	{
		if(0==strcmp((char*)(g_partition_info[i].name),"u-boot"))
		{
			aliburn_printf("found u-boot %d partion!\n",i);
			break;
		}	
	}

	if(i<g_part_num)
	{
		/*chuhua added for u-boot upgrade*/
		MakeValue(*(unsigned int *)(pmi_buf + 260+i*8), pmi_buf + 100);	
		MakeValue(*(unsigned int *)(pmi_buf + 264+i*8), pmi_buf + 104);
	}
	else
	{
		aliburn_printf("not found u-boot partion,default skip to 1 partion!\n");
		/*chuhua added for u-boot upgrade*/
		MakeValue(*(unsigned int *)(pmi_buf + 268), pmi_buf + 100);	
		MakeValue(*(unsigned int *)(pmi_buf + 272), pmi_buf + 104);
	}

	index1 = *(unsigned int *)(pmi_buf + 76);
	index2 = *(unsigned int *)(pmi_buf + 80);
	index3 = *(unsigned int *)(pmi_buf + 100);
	index4 = *(unsigned int *)(pmi_buf + 104);
	aliburn_printf("\n\n\ddr addr=0x%x, ddr len=0x%x, boot addr=0x%x, boot len=0x%x\n\n\n", index1, index2, index3, index4);
}
#endif

/*
	start 0:led on
	success 1: keep led on
	work 2: nostable led on and off 1s/1s
	fail 3:0.2s/0.2s
*/

//#define PHY_LED_GPIO	121 // S3921 DB,
#define PHY_LED_GPIO	6 // C3921 DB, for test
#define PHY_LINK_LED_GPIO	5 // C3921 DB, for test


void ali_set_led_status(int status)
{
	int led_flag=0;
	static int work_led_flag=0;
    
	switch(status)
	{
		case 0:
			gpio_direction_output(PHY_LED_GPIO, 0);
			break;
		case 1:
			gpio_direction_output(PHY_LED_GPIO, 0);
			break;
		case 2:
			work_led_flag = (work_led_flag+1)%2;
            
			gpio_direction_output(PHY_LED_GPIO,work_led_flag);
			//gpio_direction_output(121, 0);
			//udelay(500000);
			//gpio_direction_output(121, 1);
			//udelay(500000);
			break;
		case 3:
			while(1)
			{
                led_flag = (led_flag + 1)%2;
                
				gpio_direction_output(PHY_LED_GPIO, led_flag);
                gpio_direction_output(PHY_LINK_LED_GPIO, led_flag);
				udelay(100000);
			}
			break;
	}
}

static void ali_burn_persent_show(unsigned int lnFLen)
{
    int percent;

    if(!g_total_burn_size)
        return ;

    g_burn_size = g_burn_size + lnFLen;

#ifdef READ_BACK_CHECK
    //The whole burn process is consist of the burning process and the checking process;          
    //    percent = g_burn_size/(2*g_total_burn_size/100);
    percent = g_burn_size/(g_total_burn_size/50);
#else
    percent = g_burn_size/(g_total_burn_size/100);
#endif
    //printf("percent %d,%d,%d\n",percent,g_burn_size,g_total_burn_size);

//    ali_display_burn_percent(percent);
    ali_set_led_status(WORK_STATUS);

}

static int readFlashToBuf(nand_info_t *nand,unsigned char *buf,unsigned int lnFLen, unsigned long nAddrUnit, unsigned int *nRetLen)
{
	int rwsize;

	rwsize =lnFLen;
    
#ifdef SD_PINMUX_SWITCH
	ali_nand_pinmux_set( __LINE__ );
#endif	

	//if(0 != nand_read_skip_bad(nand, nAddrUnit, (size_t *)&rwsize, buf))	
	{
		printf("\n  %s(),L[%d], nAddrUnit[0x%lx], rwsize[x%x] \n",__FUNCTION__,__LINE__,nAddrUnit, rwsize);	
		return -1;
	}
	*nRetLen = rwsize;
	ali_set_led_status(WORK_STATUS);
	return 1;
}

static int writeBufToFlash(nand_info_t *nand,unsigned char *buf, unsigned int lnFLen, unsigned long nAddrUnit, unsigned int *nRetLen)
{
    unsigned int rwlen = 0;
    int ret = 0;

    rwlen = lnFLen;

#ifdef SD_PINMUX_SWITCH
    ali_nand_pinmux_set( __LINE__ );
#endif	

    //ret = nand_write_skip_bad(nand, nAddrUnit, &rwlen,buf,0);
    if(0!=ret){
        aliburn_printf("++write block_buffer to nand fail. buf[0x%x],nAddrUnit[0x%08x], rwlen[0x%08x],lnFLen[0x%x] ret[%d]**\n",buf,nAddrUnit,rwlen,lnFLen,ret);
        return -1;
    }

    ali_burn_persent_show(lnFLen);
    
    return 1;
}



/*write file to flash
   return 0 success,-1 otherwise
*/
static int writeImageToFlash(nand_info_t *nand,char *FileName, unsigned int lnFLen, unsigned int nAddrUnit, unsigned int *nRetLen)
{
    int j = 0;
    int count=0;
    int filelen=0;
    int readsize=0;
    int rwsize=0;
    unsigned long shift = 0, len = 0;
    unsigned long offset = 0;
    unsigned int readRetLen = 0;
    int ret = 0;

#ifdef DEBUG_ALI_BURN_CONSUMP
    unsigned int    start_time=0, stop_time=0;
    unsigned int    cur_time_p = 0;
    unsigned int    cur_time_rd = 0, cur_time_wr = 0;

    start_time = get_timer(0);
#endif

    if(1 == ali_get_single_file_status())
        filelen = ali_get_zip_filelength(FileName);
    else
        filelen = ali_get_filelength(FileName);	

#ifdef DEBUG_ALI_BURN_CONSUMP
    cur_time_p = get_timer(0);
#endif

    /*write file to nandflash*/
//    aliburn_printf("Filename[%s],  filelen=%d, nAddrUnit=0x%08x\n",FileName,lnFLen,nAddrUnit);
    		   
    count =0;
    for(j = 0; j < (filelen / g_BlockSize); j++){

    	//read the part of file to block_buffer
    	if(1 == ali_get_single_file_status()){
    		ret = ali_get_local_file( FileName, &shift, &len);
    		if( ret < 0 ){
    		    aliburn_printf(" %s(),L[%d], Can't find file! FileName[%s] \n", __FUNCTION__,__LINE__,FileName);  

    		    goto EXIT;
    		} 

    		readsize = ali_read_file_at(zip_file_name, shift + j*g_BlockSize, burn_block_buffer, g_BlockSize);
        }else{
    		readsize = ali_read_file_at(FileName, j*g_BlockSize, burn_block_buffer, g_BlockSize);
        }            
        
    	if(readsize<0){
    		aliburn_printf("read %s file fail **\n",FileName);
    		ret = -1;
    		goto EXIT;
    	}
   
    	//find good block
    	while(1){
    		offset = nAddrUnit + count*g_BlockSize;
    		if(0 == nand_block_isbad(nand,offset)){                           
    			count++;
    			break;
    		}
            
    		count++;
    		//if(count>=(lnFLen/g_BlockSize))
    		//{	
    		//	aliburn_printf("didn't find good block\n");
    		//	return -1;
    		//}
    	}

//        aliburn_test_printf("--- read %s file ok. readsize[%d], g_BlockSize[%d],burn_block_buffer[0x%x] \n",FileName, readsize, g_BlockSize, burn_block_buffer);
    		
    	//write block_buffer to nand
    	rwsize = g_BlockSize;
    	//printf("write block 0x%08x ,rwsize 0x%04x\n",offset,rwsize);
    	if(writeBufToFlash(nand, burn_block_buffer, rwsize, (unsigned long)offset, &readRetLen)<=0){
    		aliburn_printf("write block_buffer to nand fail. burn_block_buffer[0x%x],offset[0x%08x], rwsize[0x%08x]**\n",burn_block_buffer,offset,rwsize);
    		ret = -1;
    		goto EXIT;
    	} 
    }

    if(filelen%g_BlockSize){

    	//read the part of file to blcok_buffer
    	if(1 == ali_get_single_file_status()){
    		ret = ali_get_local_file( FileName, &shift, &len);
    		if( ret < 0 ){
    		    aliburn_printf(" %s(),L[%d],Can't find file! FileName[%s] \n", __FUNCTION__,__LINE__,FileName);  

    		    goto EXIT;
    		} 

    		readsize = ali_read_file_at(zip_file_name, shift + j*g_BlockSize, burn_block_buffer, filelen%g_BlockSize);
        } else{
            readsize = ali_read_file_at(FileName, j*g_BlockSize, burn_block_buffer, filelen%g_BlockSize);
        }            

    	if(readsize < 0){
    		aliburn_printf("read %s file fail **\n",FileName);
    		ret = -1;
    		goto EXIT;
    	}
    
    	//find good block
    	while(1){
    		offset = nAddrUnit+count*g_BlockSize;
    		if(0==nand_block_isbad(nand,offset)){
    			count++;
    			break;
    		}
                    
    		count++;
    		//if(count>=(size/g_BlockSize))
    		//{	
    		//	aliburn_printf("didn't find good block\n");
    		//	return -1;
    		//}
    	}
      
    	//write block_buffer to nand
    	rwsize = filelen%g_BlockSize;
    	//printf("write block 0x%08x ,rwsize 0x%04x\n",offset,rwsize);
    	if(writeBufToFlash(nand,burn_block_buffer, rwsize, (unsigned long)offset, &nRetLen)<=0){
    		aliburn_printf("write block_buffer to nand fail. burn_block_buffer[0x%x],offset[0x%08x], rwsize[0x%08x]**\n",burn_block_buffer,offset,rwsize);
    		ret = -1;
    		goto EXIT;
    	}        
    }

EXIT:

    return ret;
}


static int ali_block_data_cmp(unsigned char *buf_1, unsigned char *buf_2, size_t rwsize)
{
    size_t		k = 0, pos = 0;
    int		ret = 0;

#if 1
    unsigned int    data_1 = 0, data_2 = 0;
    for( k = 0; k < rwsize/4; k++ ){
        data_1 = (UINT32)(*(unsigned int *)(buf_1 + k*4));
        data_2 = (UINT32)(*(unsigned int *)(buf_2 + k*4));

        if(data_1 != data_2 ){
            aliburn_printf("cmp block data fail, data_1[0x%x], data_2[0x%x],k[%d] **\n",data_1,data_2,k);
            ret = -1;
            goto EXIT;
        }                         
    }

    pos = 4*(rwsize/4);	
    if(0 != memcmp((void *)(buf_1 + pos) , (void *)(buf_2 + pos),rwsize%4)){
        aliburn_printf("cmp block data fail, left_len[%d] **\n",rwsize%4);
        ret = -1;
        goto EXIT;
    } 	
#endif

#if 0
#define   GROUP_NUM     8
#define   CHAR_NUM       (8*4)
    int m = 0;

    unsigned int    data_1[GROUP_NUM] = {0};
    unsigned int    data_2[GROUP_NUM] = {0};
    for( k = 0; k < rwsize/CHAR_NUM; k++ ){
        for(m=0; m < GROUP_NUM; m++){
            data_1[m] = (UINT32)(*(unsigned int *)(buf_1 + k*GROUP_NUM + m*4));
            data_2[m] = (UINT32)(*(unsigned int *)(buf_2 + k*GROUP_NUM + m*4));

            if((data_1[m] != data_2[m] )){
                aliburn_printf("cmp block data fail, data_1[0x%x], data_2[0x%x],k[%d] **\n",data_1[m],data_2[m],k);
                ret = -1;
                goto EXIT;
            }                   
        }                     
    }

    pos = CHAR_NUM*(rwsize/CHAR_NUM) ;	
#if 0
    if(0 != memcmp((void *)(buf_1 + pos) , (void *)(buf_2 + pos),rwsize%16)){
        aliburn_printf("cmp block data fail, left_len[%d] **\n",rwsize%16);
        ret = -1;
        goto EXIT;
    } 	
#endif
    for( k= 0; k < rwsize%CHAR_NUM; k++){
        if(*(unsigned char *)(buf_1 + pos + k)!= *(unsigned char *)(buf_2 + pos + k)){
            aliburn_printf("cmp block data fail, left_len[%d] **\n",rwsize%CHAR_NUM);
            ret = -1;
            goto EXIT;
        } 	
    }

#endif


EXIT:

    return ret;
}    

/*******************************************************************************
* Compare the Nand flash data after writing 
*******************************************************************************/
static int ali_flash_data_cmp_unit( nand_info_t *nand ,unsigned char *mem_addr,unsigned int offset, unsigned int rwsize )
{
    unsigned int   RetLen = 0;
    int     ret = 0;

#if 0    
    unsigned int  tick_rd_start, tick_cmp_start, tick_end;
    tick_rd_start = get_timer(0);
#endif

    memset((unsigned char *)cmp_burn_block_buffer, 0xFF, BURN_BLOCK_LEN);
    if( readFlashToBuf(nand, cmp_burn_block_buffer, rwsize, (unsigned long)offset, &RetLen) < 0 ){
        aliburn_printf(" L[%d], read block to buffer fail offset 0x%08x  size 0x%08x**\n",__LINE__,offset,rwsize);
        ret = -1;
        goto EXIT;
    }

//    tick_cmp_start = get_timer(0);
    
    /*** Compare the flash data ****/
#if 0         
    if(0 != memcmp((void *)(mem_addr), cmp_burn_block_buffer,rwsize)){
        aliburn_printf("cmp bootloader block data fail **\n");
        ret = -1;
        goto EXIT;
    } 
#else
    if(0 != ali_block_data_cmp((unsigned char *)(mem_addr), (unsigned char *)cmp_burn_block_buffer,rwsize)){
        aliburn_printf("cmp bootloader block data fail **\n");
        ret = -1;
        goto EXIT;
    }                 
#endif

#if 0
    tick_end = get_timer(0);
    aliburn_printf("RdFlash [%d], CMP[%d] \n",(tick_cmp_start - tick_rd_start), (tick_end - tick_cmp_start));
#endif

EXIT:

    return ret;
    
}

/*********************************************************************
* Compare the Partition_0 & Partition_1 with bootloader
* Make sure the bootloader size is no larger than BURN_BLOCK_LEN
*********************************************************************/
static int ali_flash_bl_data_cmp(nand_info_t *nand ,char *FileName, unsigned int FileLen, unsigned int start)
{
    unsigned int	rdOffset = 0, offset = 0;
    unsigned int	readsize = 0, RetLen = 0;
    size_t 			rwsize;
    int 	j = 0 ;    
    int 	count = 0;
    int     ret = 0;

    if(FileLen <= 0){
        ret =  -1;
        goto EXIT;
    }

    /* Compare Partition_0 & Partition_1
    *  Make sure the bootloader size is no larger than BURN_BLOCK_LEN*/
    count = 0;
    rdOffset = 0;

    for( j = 0; j < (FileLen / g_BlockSize); j++ ){
        rwsize = g_BlockSize;  
        
        //find good block
        while(1){
            offset = start + count * g_BlockSize;
            if(0 == nand_block_isbad(nand, offset)){
                count++;
                break;
            }               
            count++;
        }

        ret  = ali_flash_data_cmp_unit(nand,burn_bl_block_buffer + rdOffset , offset, rwsize);
        if(ret < 0){
            aliburn_printf("cmp block data fail , rwsize[%d] **\n",rwsize);
            ret = -1;
            goto EXIT;            
        }
        
        rdOffset += rwsize;
    }

    if( FileLen % g_BlockSize){
        rwsize =  FileLen % g_BlockSize;        
        
        //find good block
        while(1){
            offset = start + count * g_BlockSize;
            if(0 == nand_block_isbad(nand, offset)){
                count++;
                break;
            }            
            count++;
        }

        ret = ali_flash_data_cmp_unit(nand, burn_bl_block_buffer + rdOffset , offset, rwsize);
        if(ret < 0){
            aliburn_printf("cmp block data fail , rwsize[%d] **\n",rwsize);
            ret = -1;
            goto EXIT;            
        }     
        
        rdOffset += rwsize;
    } 

EXIT:
    return ret;
    
}

/*********************************************************************
* Compare the other partitions except for the Partition_0 & Partition_1
* Make sure the bootloader size is no larger than BURN_BLOCK_LEN
*********************************************************************/
static int ali_flash_partition_data_cmp(nand_info_t *nand ,char *FileName, unsigned int FileLen, unsigned int start)
{
    unsigned int	offset = 0;
    unsigned int	readsize = 0, RetLen = 0;
    size_t 			rwsize;
    unsigned long 	shift = 0, len = 0;
    int 	    j = 0 ;    
    int 	    count = 0;
    int          ret = 0;

    if((FileLen <= 0)||(!FileName)){
        ret =  -1;
        goto EXIT;
    }

    /* Compare the other partitions except for the Partition_0 & Partition_1
    *  Make sure the bootloader size is no larger than BURN_BLOCK_LEN*/
    
    /*write the changed bootloader file to nandflash*/
    count = 0;
    for( j = 0; j < (FileLen / g_BlockSize); j++ ){
        rwsize = g_BlockSize;    
        
        //find good block
        while(1){                    
            offset = start + count * g_BlockSize;
            if(0 == nand_block_isbad(nand, offset)){                           
                count++;
                break;
            }  
            count++;
        }

//        printf("\n -11-s-mem-  \n");            
        /**  Read data from U-disk/SD to block_buffer **/
        memset((unsigned char *)burn_block_buffer, 0xFF, BURN_BLOCK_LEN);
        if(1 == ali_get_single_file_status()){
            ret = ali_get_local_file( FileName, &shift, &len);
            if( ret < 0 ){
                aliburn_printf(" Can't find file! FileName[%s] \n", FileName);  
                goto EXIT;
            } 
            readsize = ali_read_file_at(zip_file_name, shift + j*g_BlockSize, burn_block_buffer, rwsize);
        } else
            readsize = ali_read_file_at( FileName, j*g_BlockSize, burn_block_buffer, rwsize);               
        
        if( readsize < 0){
            aliburn_printf("read %s file fail **\n",FileName);
            ret = -1;
            goto EXIT;
        }
        
//        printf("-12-s-cmp-  \n");     

        /*** Compare the flash data ****/
        ret  = ali_flash_data_cmp_unit(nand, burn_block_buffer , offset, rwsize);
        if(ret < 0){
            aliburn_printf("cmp block data fail , rwsize[%d] **\n",rwsize);
            ret = -1;
            goto EXIT;            
        }

//        printf("-13-end-  \n"); 
        
    }

    if( FileLen % g_BlockSize){
        rwsize =  FileLen % g_BlockSize;      
        
        //find good block
        while(1){                    
            offset = start + count * g_BlockSize;                    
            if(0 == nand_block_isbad(nand, offset)){
                count++;
                break;
            }                   			        
            count++;
        }

        /**  Read data from U-disk/SD to block_buffer **/
        memset((unsigned char *)burn_block_buffer, 0xFF, BURN_BLOCK_LEN);
        if(1 == ali_get_single_file_status()){
            ret = ali_get_local_file( FileName, &shift, &len);
            if( ret < 0 ){
                aliburn_printf(" Can't find file! FileName[%s] \n", FileName);  
                goto EXIT;
            } 
            readsize = ali_read_file_at(zip_file_name, shift + (FileLen / g_BlockSize)*g_BlockSize, burn_block_buffer, rwsize);
        } else{
            readsize = ali_read_file_at( FileName, (FileLen / g_BlockSize)*g_BlockSize, burn_block_buffer, rwsize);
        }

        if( readsize < 0){
            aliburn_printf("read %s file fail **\n",FileName);
            ret = -1;
            goto EXIT;
        }

        /*** Compare the flash data ****/
        ret  = ali_flash_data_cmp_unit(nand, burn_block_buffer , offset, rwsize);
        if(ret < 0){
            aliburn_printf("cmp block data fail , rwsize[%d] **\n",rwsize);
            ret = -1;
            goto EXIT;            
        }

    } 

EXIT:
    return ret;
    
}


/*******************************************************************************
* Compare the Nand flash data after writing 
*******************************************************************************/
static int ali_flash_data_cmp( nand_info_t *nand )
{
    unsigned int	FileLen = 0;
    unsigned int	readsize = 0, RetLen = 0;
    unsigned int	rdOffset = 0, offset = 0;
    unsigned int 	start = 0, size = 0;   
    size_t 			rwsize;
    int 	i = 0, j = 0 ;    
    int 	ret = 0;
    int 	count = 0;
    char	FileName[256] = "";

    if( NULL == nand ){
        printf(" pointer variable is illegal! \n");
        ret = -1;
        goto EXIT;
    }   

    for( i = 0; i < g_part_num; i++ ){
        start = g_partition_info[i].start;
        size = g_partition_info[i].size;
        FileLen = g_partition_info[i].file.Size;
        strcpy( FileName,(char *)(g_partition_info[i].file.Name));

        aliburn_printf("\n %s(), L[%d],FileName[%s], start[0x%x], size[0x%x],FileLen[%d] \n",__FUNCTION__, __LINE__,FileName,start,size,FileLen);
        
        /*No partition need to burn*/
        if(FileLen <= 0){
            aliburn_printf(" No data to read from flash, partition[%d] \n", i);
            continue;          
        }  

    	/* Read data from the flash*/
#ifdef SD_PINMUX_SWITCH
    	ali_nand_pinmux_set( __LINE__ );
#endif

    	if( i < 2 ){
            /* Compare Partition_0 & Partition_1
            *  Make sure the bootloader size is no larger than BURN_BLOCK_LEN*/

            ret = ali_flash_bl_data_cmp(nand ,FileName, FileLen, start);
            if(ret < 0){
                aliburn_printf("cmp block data fail , partition[%d] **\n",i);
                ret = -1;
                goto EXIT;            
            }   

            ali_burn_persent_show(FileLen);
        }else{

            ret = ali_flash_partition_data_cmp(nand ,FileName, FileLen, start);
            if(ret < 0){
                aliburn_printf("cmp block data fail , partition[%d] **\n",i);
                ret = -1;
                goto EXIT;            
            }   

            ali_burn_persent_show(FileLen);
        }           
    }

EXIT:
    
    return ret;

}  


#ifdef C3921_BOOTAREA
/*******************************************************************************
* Only burn flash about Partition_0 & Partition_1 with Bootloader and BootArea
*******************************************************************************/
 #if defined(CONFIG_ALI_MMC)
#if defined(CONFIG_ALI_SD)
int ali_bl_bootarea_write(nand_info_t *nand, int idx)
{
    unsigned int wrOffset = 0;
    unsigned int FileLen = 0;
    unsigned int RetLen=0;
    unsigned int offset = 0;
    unsigned int start = 0,size = 0;
    unsigned long readsize = 0, shift = 0; 
    size_t		rwsize;
    int 		ret = 0;
    int 		good_count = 0;
    int 		count=0;
    int 		j = 0;
    unsigned short *pBootParams = NULL;
    static int flag = 0;

    if( NULL == nand ){
        printf(" pointer variable is illegal! \n");
        ret = -1;
        goto EXIT;
    }     

    if( idx > 1 ){
        printf(" Partition Idx is too large! \n");	
        ret = -1;
        goto EXIT;
    }   

	/*  To Get new bootloader for partition_0 and partition_1  **/
    if( 0 == flag ){
        pBootParams = (unsigned short *)burn_bl_block_buffer;
        memset((unsigned char *)pBootParams, 0xFF, g_LoaderLength);

        aliburn_printf(" %s(),L[%d], file[%s] \n",__FUNCTION__, __LINE__, g_loaderfile);

        if(1 == ali_get_single_file_status())
            readsize = ali_read_zip_file(g_loaderfile, zip_file_name, burn_bl_block_buffer);        
        else
            readsize = ali_read_file(g_loaderfile, burn_bl_block_buffer);

        if( readsize < 0){
            aliburn_printf("read loader file error **\n");
            ret = -1;
            goto EXIT;
        }

        /*  To Change Bootloader according to Nand Info **/
        //arthurc3921  add nand info of loader with sig to aux area 
        //if (cf.Open(hWnd->m_szLoaderFileName, CFile::modeRead | CFile::shareDenyWrite) == FALSE) 
            
        memset((unsigned char *)(pBootParams+(0x290/2)), 0x00, 64);

        *(unsigned int *)(pBootParams + (0x290)/2) = g_nand_info.dwBlocksPerChip;
        *(unsigned int *)(pBootParams + (0x290+4)/2) = g_PageSize;
        *(unsigned int *)(pBootParams + (0x290+8)/2) = g_nand_info.dwPagesPerBlock;
        *(unsigned int *)(pBootParams + (0x290+36)/2) = g_nand_info.bReadCycle;
        *(unsigned int *)(pBootParams + (0x290+40)/2) = g_nand_info.bWriteCycle;
        // update Boot Params Nand Info
        switch (g_nand_info.bEccMode)//ECC mode
        {
            case ECC_60BIT:
                *(pBootParams + (0x2D0/2)) = 0xFFFF; *(pBootParams + (0x2D2/2)) = 0x0000; *(pBootParams + (0x2D4/2)) = 0xFFFF;
                break;

            case ECC_48BIT:
                *(pBootParams + (0x2D0/2)) = 0x0000; *(pBootParams + (0x2D2/2)) = 0x0000; *(pBootParams + (0x2D4/2)) = 0xFFFF;
                break;

            case ECC_40BIT:
                *(pBootParams + (0x2D0/2)) = 0xFFFF; *(pBootParams + (0x2D2/2)) = 0xFFFF; *(pBootParams + (0x2D4/2)) = 0x0000;
                break;

            case ECC_24BIT:
                *(pBootParams + (0x2D0/2)) = 0x0000; *(pBootParams + (0x2D2/2)) = 0xFFFF; *(pBootParams + (0x2D4/2)) = 0x0000;
                break;

            case ECC_16BIT:
                *(pBootParams + (0x2D0/2)) = 0xFFFF; *(pBootParams + (0x2D2/2)) = 0x0000; *(pBootParams + (0x2D4/2)) = 0x0000;
                break;

            default:                    
                break;
        }
        switch (g_PageSize)//page size
        {
            case 16*1024:
                *(pBootParams + (0x2D6/2)) = 0x0000; *(pBootParams + (0x2D8/2)) = 0x0000;
                break;

            case 8*1024:
                *(pBootParams + (0x2D6/2)) = 0xFFFF; *(pBootParams + (0x2D8/2)) = 0x0000;
                break;

            case 4*1024:
                *(pBootParams + (0x2D6/2)) = 0x0000; *(pBootParams + (0x2D8/2)) = 0xFFFF;
                break;

            case 2*1024:
                *(pBootParams + (0x2D6/2)) = 0xFFFF; *(pBootParams + (0x2D8/2)) = 0xFFFF;
                break;

            default:                
                break;
        }
        switch (g_nand_info.dwPagesPerBlock)//pages per block
        {
            case 64:
                *(pBootParams + (0x2DA/2)) = 0x0000; *(pBootParams + (0x2DC/2)) = 0x0000;
                break;

            case 128:
                *(pBootParams + (0x2DA/2)) = 0xFFFF; *(pBootParams + (0x2DC/2)) = 0x0000;
                break;

            case 192:
                *(pBootParams + (0x2DA/2)) = 0xFFFF; *(pBootParams + (0x2DC/2)) = 0x0000;
                break;

            case 256:   
                *(pBootParams + (0x2DA/2)) = 0xFFFF; *(pBootParams + (0x2DC/2)) = 0xFFFF;
                break;

            default:                
                break;
        }

        // update Aux nand retry mode
        *(pBootParams + (0x2E0/2)) = 0x0000; *(pBootParams + (0x2E2/2)) = 0x0000; *(pBootParams + (0x2E4/2)) = 0x0000; *(pBootParams + (0x2E6/2)) = 0x0000;
        *(pBootParams + (0x2E8/2)) = 0x0000; *(pBootParams + (0x2EA/2)) = 0x0000; *(pBootParams + (0x2EC/2)) = 0x0000; *(pBootParams + (0x2EE/2)) = 0x0000;
        // update Aux nand ID
        *(pBootParams + (0x2F0/2)) = 0xFFFF; *(pBootParams + (0x2F2/2)) = 0x0000; *(pBootParams + (0x2F4/2)) = 0x0000; *(pBootParams + (0x2F6/2)) = 0xFFFF;
        *(pBootParams + (0x2F8/2)) = 0xFFFF; *(pBootParams + (0x2FA/2)) = 0xFFFF; *(pBootParams + (0x2FC/2)) = 0x0000; *(pBootParams + (0x2FE/2)) = 0x0000;
        *(pBootParams + (0x300/2)) = 0xFFFF; *(pBootParams + (0x302/2)) = 0x0000; *(pBootParams + (0x304/2)) = 0x0000; *(pBootParams + (0x306/2)) = 0x0000;
        *(pBootParams + (0x308/2)) = 0x0000; *(pBootParams + (0x30A/2)) = 0xFFFF; *(pBootParams + (0x30C/2)) = 0x0000; *(pBootParams + (0x30E/2)) = 0x0000;
        *(pBootParams + (0x310/2)) = 0xFFFF; *(pBootParams + (0x312/2)) = 0x0000; *(pBootParams + (0x314/2)) = 0xFFFF; *(pBootParams + (0x316/2)) = 0x0000;
        *(pBootParams + (0x318/2)) = 0xFFFF; *(pBootParams + (0x31A/2)) = 0x0000; *(pBootParams + (0x31C/2)) = 0xFFFF; *(pBootParams + (0x31E/2)) = 0x0000;
        *(pBootParams + (0x320/2)) = 0x0000; *(pBootParams + (0x322/2)) = 0xFFFF; *(pBootParams + (0x324/2)) = 0x0000; *(pBootParams + (0x326/2)) = 0xFFFF;
        *(pBootParams + (0x328/2)) = 0x0000; *(pBootParams + (0x32A/2)) = 0xFFFF; *(pBootParams + (0x32C/2)) = 0x0000; *(pBootParams + (0x32E/2)) = 0xFFFF;

        flag = 1;
    }        

    /*** Judge the Bad Block numbers and burn data into flash ***/
    start = g_partition_info[idx].start;
    size = g_partition_info[idx].size;
    FileLen = g_partition_info[idx].file.Size;

    	/*No partition need to burn*/
    if(FileLen <= 0){
        aliburn_printf(" No data to burn flash \n");
        ret = 0;
        goto EXIT;			
    }   

    aliburn_test_printf(" start[0x%x], size[0x%x],FileLen[%d],g_BlockSize[0x%x]\n",start,size,FileLen,g_BlockSize);

    /*cal the good blocks of the partion*/
    good_count =0;   
    for(j = 0; j < (size / g_BlockSize); j++){	
        if (nand_block_isbad(nand, start + j * g_BlockSize )){
        	aliburn_printf("address 0x%08x is bad block\n",start+j*g_BlockSize);
        }else{
        	good_count++;
        	if( good_count*g_BlockSize > FileLen )
                break;            
        }					
    }

    if( FileLen > good_count * g_BlockSize){
    	aliburn_printf("no enough blocks to store partion. FileLen[0x%x, valid_block_cnt[%d]\n", FileLen, good_count);
    	ret = -1;
    	goto EXIT;
    }else{
        if(g_BlockSize > BURN_BLOCK_LEN){
            aliburn_printf("Nand Flash Block Size > %dM\n",BURN_BLOCK_LEN);
            ret = -1;
            goto EXIT;
        }

    	/*erase partition*/
#ifdef SD_PINMUX_SWITCH
        ali_nand_pinmux_set( __LINE__ );
#endif
        aliburn_printf(" nand erase start 0x%08x size 0x%08x \n",start, size);
        ret = nand_erase_partition(nand, start, size);
        if( ret < 0){
        	aliburn_printf("  %s(), L[%d], nand_erase_partition fail, start[0x%08x], size[0x%08x], ret[%d] \n",__FUNCTION__,__LINE__,start, size,ret);			
        }

        /*write the changed bootloader file to nandflash*/
        count = 0;
        for( j = 0; j < (FileLen / g_BlockSize); j++ ){
//			aliburn_test_printf(" %s(), L[%d],FileLen[%d],g_BlockSize[0x%x],j[%d]\n",__FUNCTION__, __LINE__,FileLen,g_BlockSize,j); 

            //find good block
            while(1){
                offset = start + count * g_BlockSize;
                if(0 == nand_block_isbad(nand, offset)){                           
                	count++;
                	break;
                }               
                count++;
            }

            rwsize = g_BlockSize;

            //printf("write block 0x%08x ,rwsize 0x%04x\n",offset,rwsize);
            if( writeBufToFlash(nand, burn_bl_block_buffer + wrOffset, rwsize, (unsigned long)offset, &RetLen) <= 0 ){
                aliburn_test_printf(" write block_buffer to nand fail. burn_bl_block_buffer[0x%x],offset[0x%08x], rwsize[0x%08x]**\n",burn_bl_block_buffer,offset,rwsize);
                ret = -1;
                goto EXIT;
            } 

            wrOffset += rwsize ;
        }

    	if( FileLen % g_BlockSize)
    	{
//            aliburn_test_printf("  %s(), L[%d],FileLen[%d],g_BlockSize[0x%x]\n",__FUNCTION__, __LINE__,FileLen,g_BlockSize ); 

            //find good block
            while(1){
                offset = start + count * g_BlockSize;
                if(0==nand_block_isbad(nand, offset)){
                	count++;
                	break;
                }                		     
                count++;
            }

            rwsize =  FileLen % g_BlockSize;		

            //printf("write block 0x%08x ,rwsize 0x%04x\n",offset,rwsize);
            if( writeBufToFlash(nand, burn_bl_block_buffer + wrOffset, rwsize, (unsigned long)offset, &RetLen) <= 0 ){
                aliburn_test_printf(" write block_buffer to nand fail. burn_bl_block_buffer[0x%x],offset[0x%08x], rwsize[0x%08x]**\n",burn_bl_block_buffer,offset,rwsize);
                ret = -1;
                goto EXIT;
            } 

            wrOffset += rwsize ;            		    
    	} 

    	aliburn_test_printf(" %s(), L[%d],Success ,File[%s],FileLen[%d], wrOffset[%d] \n \n",__FUNCTION__, __LINE__, g_partition_info[idx].file.Name,FileLen,wrOffset);
        
    }

EXIT:
    return ret;
    
}    
#endif
#endif
#endif

#ifdef SUPPORT_SINGLE_FILE


/***********************************************************************************
*FUNCTION: Used to get the total information about the total files,File Name 
*			and addrss offset
*Input:
*  *cf : The input file
*  *pRecord : The Structure Pointer to save the partition info
* OutPut:
* 	ret: -1, Fail
*        0, Success
***********************************************************************************/
//BOOL CZipStore::GetCentralDirRecord(CFile &cf, EndOfCentralDirRecord *pRecord)
 #if defined(CONFIG_ALI_MMC)
#if defined(CONFIG_ALI_SD)
int ali_get_central_dir_record 
(
	char *cf, 
	EndOfCentralDirRecord *pRecord
)
{
	unsigned char 	pBuf[BUFREADCOMMENT + 4];
	unsigned long 	uSizeFile = 0;
	unsigned long	uBackRead = 0;
	unsigned long	uMaxBack = 0xFFFFFFFF; /* maximum size of global comment */
	unsigned long 	uPosFound = 0;
    unsigned long 	uReadSize = 0, uReadPos = 0;
    unsigned long 	len = 0, pos = 0;    
	unsigned short 	CommentLength = 0;
    int i = 0 ; 
 	int ret = 0;


	uSizeFile = ali_get_filelength(cf);

	if (uMaxBack > uSizeFile)
		uMaxBack = uSizeFile;

	uBackRead = 4;
	while (uBackRead < uMaxBack)
	{
		if (( uBackRead + BUFREADCOMMENT ) > uMaxBack)
			uBackRead = uMaxBack;
		else
			uBackRead += BUFREADCOMMENT;
        
		uReadPos = uSizeFile - uBackRead;
        
		uReadSize = ((BUFREADCOMMENT + 4) < (uSizeFile - uReadPos)) ?
					 (BUFREADCOMMENT + 4) : (unsigned long)(uSizeFile - uReadPos);

//		cf.Seek(uReadPos, CFile::begin);
//		if (cf.Read(pBuf, uReadSize) != uReadSize)

		memset((unsigned char *)pBuf, 0x00, BUFREADCOMMENT + 4);
		len = ali_read_file_at(cf, uReadPos, pBuf, uReadSize);            
		if ( len != uReadSize )
		{
			aliburn_printf("%s(),L[%d],Read zip file at pos[0x%x] fail, len[%d] \n",__FUNCTION__,__LINE__,uReadPos, len);
            ret = -1;
            goto RECORD_EXIT;
		}

		for (i = (int)uReadSize - 3; (i--) > 0;)
		{
			if ( CHECK_SIG(pBuf + i, EndOfCentralDirRecordSig) )
			{
				uPosFound = uReadPos + i;
				break;
			}
		}

		if ( uPosFound != 0 )
			break;
	}

	if ( 0 == uPosFound )
	{
		aliburn_printf("Not find the info about the partition \n");

	    ret = -1;
	    goto RECORD_EXIT;
	}     

	pos = (unsigned long)(pBuf + i);
	pRecord->m_EndOfCentralDirRecordSig = READ_LONG(pos);  
    pRecord->m_NumberOfDisk = READ_SHORT(pos + 4);
	pRecord->m_DiskCentralDirStart = READ_SHORT(pos + 4 + 2);
	pRecord->m_CentralDirRecordsNumber = READ_SHORT(pos + 4 + 2*2);
	pRecord->m_CentralDirRecordsTotalNumber = READ_SHORT(pos + 4 + 2*3);
	pRecord->m_CentralDirSize = READ_LONG(pos + 4 + 2*4);
	pRecord->m_CentralDirOffset = READ_LONG(pos + 4*2 + 2*4);

    memset((unsigned char *)(pRecord->m_szComment), 0x00, COMMENT_LEN );
	pos += 20;
	CommentLength = READ_SHORT(pos + 20);
	if (CommentLength > 0)
	{  
//        strcpy((unsigned char *)(pRecord->m_szComment), (unsigned char *)(pos + 2));
		pos += 2;
		len  = 0;

        do{
			 pRecord->m_szComment[len] = pBuf[pos+len];             
             len++;
             
        }while(len < CommentLength);       
	}

RECORD_EXIT:
    
	return ret;	
}
#endif
#endif
/***********************************************************************
*Function: To get the specific info for different file component
* 
* Return Value: 
*	ret : The number of the component file
************************************************************************/
int ali_get_central_dir_file_header
(
	unsigned char *pBuf, 
	CentralDirFileHeader *pHeader, 
	unsigned long nLength 
)
{
	unsigned long pos = 0, i = 0, j = 0;
	unsigned short Length, AccLen = 0;
	char *pStr, bk;

	do
	{
		if (CHECK_SIG(pBuf + pos, CentralDirFileHeaderSig))
		{
			pHeader[i].m_CentralDirFileHeaderSig = CentralDirFileHeaderSig;
			pos += 4;
			pHeader[i].m_VersionMade = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;			
			pHeader[i].m_VersionNeeded = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;
			pHeader[i].m_Flag = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;
			pHeader[i].m_CompressionMethod = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;
			pHeader[i].m_Time = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;
			pHeader[i].m_Date = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;
			pHeader[i].m_Crc32 = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8) + \
				((*(pBuf + pos + 2)) << 16) + ((*(pBuf + pos + 3)) << 24);
			pos += 4;
			pHeader[i].m_CompressedSize = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8) + \
				((*(pBuf + pos + 2)) << 16) + ((*(pBuf + pos + 3)) << 24);
			pos += 4;
			pHeader[i].m_UncompressedSize = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8) + \
				((*(pBuf + pos + 2)) << 16) + ((*(pBuf + pos + 3)) << 24);
			pos += 4;

			//File name
			Length = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;
            
            memset((unsigned char *)(pHeader[i].m_szFileName), 0x00, COMMENT_LEN );
			if (Length > 0)
			{
				pStr = (char *)pBuf + pos + 16;
				bk = *((char *)pBuf + pos + 16 + Length);
				*(pBuf + pos + 16 + Length) = 0;

				j = 0;
                do{
                     pHeader[i].m_szFileName[j] = *((unsigned char *)(pStr + j));             
                     j++;
                     
                }while(j < Length);    

				*((char *)pBuf + pos + 16 + Length) = bk;
				AccLen = Length;
			}
			else
			{
				aliburn_printf(" No File name info \n");
			}

			// Extra field, Skipped
			Length = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;
			AccLen += Length;
			// Comment
			Length = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;
            
			memset((unsigned char *)(pHeader[i].m_szComment), 0x00, COMMENT_LEN );            
			if (Length > 0)
			{
				pStr = (char *)pBuf + pos + 12 + AccLen;
				bk = *((char *)pBuf + pos + 12 + AccLen + Length);

				j = 0;
                do{
                     pHeader[i].m_szComment[j] = *((unsigned char *)(pStr + j));             
                     j++;
                     
                }while(j < Length);   

				*((char *)pBuf + pos + 16 + AccLen + Length) = bk;
				AccLen += Length;
			}
			else
			{
//				aliburn_printf(" No File comment info \n");
			}

			pHeader[i].m_DiskNumberStart = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;
			pHeader[i].m_InternalFileAttributes = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
			pos += 2;
			pHeader[i].m_ExternalFileAttributes = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8) + \
				((*(pBuf + pos + 2)) << 16) + ((*(pBuf + pos + 3)) << 24);
			pos += 4;
			pHeader[i].m_LocalFileHeaderOffset = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8) + \
				((*(pBuf + pos + 2)) << 16) + ((*(pBuf + pos + 3)) << 24);
			pos += 4;

			pos += AccLen;

			i++;
		}
		else
		{
			pos++;
		}
	}while (pos < nLength);

	return i;
}

/**********************************************************************************************
* Function get specific info for the componant file
* 
***********************************************************************************************/
//BOOL CZipStore::GetLocalFileHeader(unsigned char *pBuf, LocalFileHeader *pHeader)
int ali_get_local_file_header
( 
	unsigned char *pBuf, 
	LocalFileHeader *pHeader
)
{
	unsigned long pos = 0, i = 0;
	unsigned short NameLength, ExtraLength;
	char *pStr, bk;    
    int 	ret = 0;

	if (!CHECK_SIG(pBuf + pos, LocalFileHeaderSig))
	{
	    aliburn_printf("Local file header signature error!\n");

        ret = -1;
        goto HEADER_EXIT;
	}

	pHeader->m_LocalFileHeaderSig = LocalFileHeaderSig;
	pos += 4;
	pHeader->m_VersionNeeded = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
	pos += 2;
	pHeader->m_Flag = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
	pos += 2;
	pHeader->m_CompressionMethod = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
	pos += 2;
	pHeader->m_Time = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
	pos += 2;
	pHeader->m_Date = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
	pos += 2;
	pHeader->m_Crc32 = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8) + \
	    ((*(pBuf + pos + 2)) << 16) + ((*(pBuf + pos + 3)) << 24);
	pos += 4;
	pHeader->m_CompressedSize = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8) + \
	    ((*(pBuf + pos + 2)) << 16) + ((*(pBuf + pos + 3)) << 24);
	pos += 4;
	pHeader->m_UncompressedSize = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8) + \
	    ((*(pBuf + pos + 2)) << 16) + ((*(pBuf + pos + 3)) << 24);
	pos += 4;


	//File name
	NameLength = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
	pos += 2;
    
	memset((unsigned char *)(pHeader->m_szFileName), 0x00, COMMENT_LEN );     
	if (NameLength > 0)
	{
	    pStr = (char *)pBuf + pos + 2;
	    bk = *((char *)pBuf + pos + 2 + NameLength);
	    *(pBuf + pos + 2 + NameLength) = 0;
        
//	    pHeader->m_szFileName = CString(pStr);
        i = 0;
        do{
             pHeader->m_szFileName[i] = *((unsigned char *)(pStr + i));             
             i++;            
        }while(i < NameLength);   
        
	    *((char *)pBuf + pos + 2 + NameLength) = bk;

	    if (*((char *)pBuf + pos + 1 + NameLength) == '/')
	        pHeader->m_bIsDir = 1;
	    else
	        pHeader->m_bIsDir = 0;
	}
	else
	{
	    pHeader->m_bIsDir = 0;
		aliburn_printf(" No File name info \n");
	}

	//Extra field
	ExtraLength = (*(pBuf + pos)) + ((*(pBuf + pos + 1)) << 8);
	pos += 2;

	pHeader->m_DataOffset = pos + NameLength + ExtraLength;

HEADER_EXIT:

	return ret;
}




/*************************************************************************************
* Function: Get the info for the component info through the single zip file
*
*************************************************************************************/
 #if defined(CONFIG_ALI_MMC)
#if defined(CONFIG_ALI_SD)
int ali_parse_zip( char *szFile )
{
	int 	i = 0;
	int		len = 0;
//	unsigned char 	pBuf[1024*1024];
    unsigned char   *pBuf = NULL;
    unsigned char   buffer[BUFREADCOMMENT];
	unsigned long 	CentralDirSize = 0, offset = 0;
	LocalFileHeader pFileHeader;
    int 	ret = 0;

    /*** Analyse the total info about local specific files ***/  
	ret = ali_get_central_dir_record( szFile, &m_CentralDirRecord);
	if (ret < 0 )
	{
		aliburn_printf(" Fail to get total number of the component files \n");
        goto ZIP_EXIT;
	}

	m_DirFileCount = m_CentralDirRecord.m_CentralDirRecordsTotalNumber;
	CentralDirSize = m_CentralDirRecord.m_CentralDirSize;

//    aliburn_printf("\n %s(),L[%d], m_DirFileCount[%d], CentralDirSize[0x%x] \n",__FUNCTION__,__LINE__,m_DirFileCount,CentralDirSize);
    
    pBuf = (unsigned char *)malloc(CentralDirSize);
    if( NULL == pBuf )
	{
		aliburn_printf("malloc buffer fail. length[0x%x] \n",CentralDirSize);
    }        

	len  = ali_read_file_at(szFile, m_CentralDirRecord.m_CentralDirOffset , pBuf, CentralDirSize);
	if( len < 0 )
	{
		aliburn_printf(" %s(), L[%d], read file fail. FileName[%s],Pos[0x%x],Readlen[0x%x] \n",\
          			__FUNCTION__,__LINE__, szFile, m_CentralDirRecord.m_CentralDirOffset , CentralDirSize);

        ret = -1;
        goto ZIP_EXIT;
	}

    /*** Analyse the offset info about local specific files ***/
	memset((unsigned char *)m_pCentralDirFileHeader, 0x00, MAX_PARTITION*sizeof(CentralDirFileHeader));    
	if (ali_get_central_dir_file_header(pBuf, &m_pCentralDirFileHeader, CentralDirSize) != m_DirFileCount)
	{

        aliburn_printf("Fail to get all the compent file \n");

        ret = -1;
		goto ZIP_EXIT;
	}

	if( NULL != pBuf )
	{
		free( (void *)pBuf );
        pBuf = NULL;
    }		


	/*** Analyse the local specific file info ***/
//	pBuf = new unsigned char [BUFREADCOMMENT];
	for (i = 0; i < m_DirFileCount; i++)
	{
		memset((void *)buffer, 0, BUFREADCOMMENT);
        
		offset = m_pCentralDirFileHeader[i].m_LocalFileHeaderOffset;
        len  = ali_read_file_at(szFile, (int)offset, buffer, BUFREADCOMMENT);
        if( len < 0 )
        {
            aliburn_printf(" %s(), L[%d], read file fail. FileName[%s],Pos[0x%lx],Readlen[0x%x],file_idx[%d] \n",\
                        __FUNCTION__,__LINE__, szFile, offset, BUFREADCOMMENT, i);
        
//            ret = -1;  	//????
//            goto ZIP_EXIT;
			continue;
        }

		if ( ali_get_local_file_header(buffer, &pFileHeader) < 0 )
		{
            memset((void *)(&m_pCentralDirFileHeader[i].m_pLocalFileHeader), 0x00, sizeof(LocalFileHeader));
			continue;
		}

		memcpy((void *)(&m_pCentralDirFileHeader[i].m_pLocalFileHeader), &pFileHeader, sizeof(LocalFileHeader));
	}

ZIP_EXIT:

    if( NULL != pBuf )
        free( (void *)pBuf );

#if 0
	//Print ZIP header info
	printf("\n %s(),L[%d], ZIP INFO: \n",__FUNCTION__,__LINE__);

	for (i = 0; i < m_DirFileCount; i++)
	{		
		printf("IDX[%d], m_szFileName[%s], m_LocalFileHeaderOffset[0x%x],local_m_szFileName[%s], m_DataOffset[0x%x] \n",\
          		i, m_pCentralDirFileHeader[i].m_szFileName, \
          		m_pCentralDirFileHeader[i].m_LocalFileHeaderOffset, \
          		m_pCentralDirFileHeader[i].m_pLocalFileHeader.m_szFileName, \
          		m_pCentralDirFileHeader[i].m_pLocalFileHeader.m_DataOffset );
	}
    printf("\n ");
#endif

	return ret;
}

#endif
#endif
/******************************************************************************************
* Function: Get the info about length and offset of the component file in the zip package 
* Input Parameter:
* 	*szfile : The fine name string pointer
* Output:
* 	Return 0 success,-1 otherwise
******************************************************************************************/
//BOOL CZipStore::GetLocalFile(CString szFile, unsigned long *pOffset, unsigned long *pLength)
int ali_get_local_file(char *ini_name, unsigned long *pOffset, unsigned long *pLength)
{
    int 	i = 0;
    int		ret = 0;
    LocalFileHeader *pFileHeader = NULL;

    if (pOffset == NULL || pLength == NULL)
    {
        aliburn_printf("Invalid parameters!\n");

        ret = -1; 
        
        goto FILE_EXIT;
    }

	for (i = 0; i < m_DirFileCount; i++)
	{
		ret = strcmp(ini_name, (char *)(m_pCentralDirFileHeader[i].m_pLocalFileHeader.m_szFileName));
		if( 0 == ret )        
		{
			pFileHeader = &m_pCentralDirFileHeader[i].m_pLocalFileHeader;		
            *pOffset = m_pCentralDirFileHeader[i].m_LocalFileHeaderOffset + pFileHeader->m_DataOffset;
            *pLength = pFileHeader->m_UncompressedSize;
            
			break;
		}
	}

    if( i >= m_DirFileCount )
	{
		aliburn_printf(" Can't find file! FileName[%s] , TotalFileCnt[%ud] \n", ini_name, m_DirFileCount);
        
		ret = -1;        
        goto FILE_EXIT;
    }        

FILE_EXIT:
    
    return ret;
}



#endif

