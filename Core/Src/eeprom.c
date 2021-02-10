/*
 * eeprom.c
 *
 *  Created on: 2020年12月8日
 *      Author: hly66
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "main.h"
#include "eeprom.h"
#include "mtp.h"

#define LOG_TAG              "eeprom"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

#define   E1_CARD_VERSION   0x1
#define   FLASH_DEBUG

//FLASH起始地址
#define STM32_FLASH_BASE 0x08000000 	//STM32 FLASH的起始地址
#define FLASH_WAITETIME  50000          //FLASH等待超时时间

#ifdef FLASH_1M
#define ADDR_FLASH_SECTOR    (0x080E0000) 	//扇区11起始地址,128 Kbytes, 选用此扇区存储参数
#else
#define ADDR_FLASH_SECTOR    (0x080E0000) 	//扇区7起始地址,128 Kbytes, 选用此扇区存储参数
#endif

e1_params_t	e1_params;
ram_params_t ram_params;
slot_t  slot_params[SLOT_MAX];
u8_t  group_user[81];

void update_eeprom(void)
{
	FLASH_EraseInitTypeDef FlashEraseInit;
	HAL_StatusTypeDef flashStatus = HAL_OK;
	uint32_t SectorError;

	HAL_FLASH_Unlock();             //解锁
	FlashEraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;       //擦除类型，扇区擦除
	FlashEraseInit.Sector = ADDR_FLASH_SECTOR;   				//要擦除的扇区
	FlashEraseInit.NbSectors = 1;                             //一次只擦除一个扇区
	FlashEraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;      //电压范围，VCC=2.7~3.6V之间!!
	if(HAL_FLASHEx_Erase(&FlashEraseInit,&SectorError) != HAL_OK){
		LOG_E("ERASE FLASH FAILED!");
		return;
	}

	flashStatus=FLASH_WaitForLastOperation(FLASH_WAITETIME);
	if (flashStatus == HAL_OK){
		uint32_t *pbuf = (uint32_t *)&e1_params;
		uint16_t cn_32 = sizeof(e1_params)/4 + 1;
		for (uint16_t i = 0; i < cn_32; i++)
			HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, ADDR_FLASH_SECTOR+i*4, pbuf[i]);
	}

	HAL_FLASH_Lock();
  LOG_D("eeprom update succeed!");
}

void reload_eeprom(void)
{
	e1_params = *(e1_params_t *)ADDR_FLASH_SECTOR;
}


void init_eeprom(void)
{
	//reload_eeprom();

    if (e1_params.version != E1_CARD_VERSION) {
        LOG_W("Start default param set!");
        memset(&e1_params, 0, sizeof(e1_params));
        e1_params.version = E1_CARD_VERSION;
        
        for( int i = 0; i < E1_CARDS; i++) {
            e1_params.e1_enable[i] = 0x02;
            e1_params.e1_l2_alarm_enable[i] = 0xff;
            e1_params.e1_port_type[i] = SS7_PORT;
            e1_params.isdn_port_type[i] = PRI_CPE;
            e1_params.pll_src[i] = 0x8;
            e1_params.crc4_enable[i] = CRC4_DISABLE;
            e1_params.no1_enable[i] = NO1_DISABLE;
        }
        
        e1_params.tone_cadence0[0] = 3;
        
        e1_params.tone_cadence0[1] = 100;
        e1_params.tone_cadence1[1] = 100;
        e1_params.tone_cadence2[1] = 100;
        e1_params.tone_cadence3[1] = 100;
        e1_params.tone_cadence4[1] = 100;
        e1_params.tone_cadence5[1] = 100;
        e1_params.tone_cadence6[1] = 100;
        e1_params.tone_cadence7[1] = 100;
        
        e1_params.tone_cadence0[2] = 0xff;
        e1_params.tone_cadence0[3] = 0xff;
        e1_params.tone_cadence0[4] = 0xff;
        memset(&e1_params.tone_cadence0[5], 0, 18-5);

        //update_eeprom();
    }
}

#if 0
static void FlashTestTask(void *argument)
{
  init_eeprom();
  //int i = 0;
  printf("Start update eeprom!\r\n");
  //LWIP_ERROR("check lwip_error", (i != 0), NULL;);
  update_eeprom();

  memset(&e1_params, 0, sizeof(e1_params));

  reload_eeprom();
  printf("e1_enable=%x\r\n", e1_params.e1_enable[0]);
  printf("e1 tone source = %d\r\n", e1_params.tone_src);
  for(;;)
  {
    osDelay(1000);
  }

}

void start_flash_test_task(void)
{
	osThreadId_t testTaskHandle;
	const osThreadAttr_t testTask_attributes = {
	  .name = "flashTask",
	  .priority = (osPriority_t) osPriorityNormal,
	  .stack_size = 128 * 4
	};

	testTaskHandle = osThreadNew(FlashTestTask, NULL, &testTask_attributes);

}
#endif
