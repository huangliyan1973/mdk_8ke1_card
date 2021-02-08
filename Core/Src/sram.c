#include "main.h"
#include "sram.h"

#define LOG_TAG              "sram"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

void sram_test(void)
{
    u8_t *sram8 = (u8_t *)SRAM_BASE_ADDR;
	u32_t i=0;  	  
	u8_t temp=0;	   
	u8_t sval=0;	//�ڵ�ַ0����������	  				   
	//ÿ��4K�ֽ�,д��һ������,�ܹ�д��256������,�պ���1M�ֽ�
	for(i=0;i<1024*1024;i+=4096)
	{
        sram8[i] = temp;
		temp++;
	}
	//���ζ���֮ǰд�������,����У��		  
 	for(i=0;i<1024*1024;i+=4096) 
	{
  		sval = sram8[i];
        if (sval != (u8_t)(i/4096)) {
            LOG_E("data error on addr:%d, value:%d", i, sval);
            goto fault; 
        }
 	}					 

    u32_t *sram32 = (u32_t *)SRAM_BASE_ADDR;
    
    *sram32 = 0x12345678;
    *(sram32+1) = 0x4567abcd;
    
    LOG_D("68000 set 0x123456 and 0x4567abcd, now read it = %08x, %08x\n", *sram32,*(sram32+1));

    LOG_I("SRAM TEST OK!");
    return;

fault:
    LOG_E("SRAM test failed!");
}
