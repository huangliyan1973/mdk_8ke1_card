/*
 * eeprom.c
 *
 *  Created on: 2020��12��8��
 *      Author: hly66
 */

#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include "main.h"
#include "eeprom.h"
#include "mtp.h"
#include "ds26518.h"

#define LOG_TAG              "eeprom"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

#define   E1_CARD_VERSION   0x1
#define   FLASH_DEBUG

//FLASH��ʼ��ַ
#define STM32_FLASH_BASE 0x08000000 	//STM32 FLASH����ʼ��ַ
#define FLASH_WAITETIME  50000          //FLASH�ȴ���ʱʱ��

#ifdef FLASH_1M
#define ADDR_FLASH_SECTOR    (0x080E0000) 	//����11��ʼ��ַ,128 Kbytes, ѡ�ô������洢����
#else
#define ADDR_FLASH_SECTOR    (0x080E0000) 	//����7��ʼ��ַ,128 Kbytes, ѡ�ô������洢����
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

	HAL_FLASH_Unlock();             //����
	FlashEraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;       //�������ͣ���������
	FlashEraseInit.Sector = ADDR_FLASH_SECTOR;   				//Ҫ����������
	FlashEraseInit.NbSectors = 1;                             //һ��ֻ����һ������
	FlashEraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;      //��ѹ��Χ��VCC=2.7~3.6V֮��!!
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

void init_tone_cadence(void)
{
  /*������*/
  u8_t *pdata = &e1_params.tone_cadence0[0];
  pdata[0] = 16;
  pdata[1] = 100;
  pdata[2] = 0xff;
  pdata[3] = 0xff;
  pdata[4] = 0xff;
  memset(&pdata[5], 0, 13);

  /*æ��*/
  pdata = &e1_params.tone_cadence1[0];
  pdata[0] = 16;
  pdata[1] = 14;
  pdata[2] = 0xfe;
  memset(&pdata[3], 0, 15);

  /*�պ���*/
  pdata = &e1_params.tone_cadence2[0];
  pdata[0] = 16;
  pdata[1] = 14;
  pdata[2] = 0xfe;
  memset(&pdata[3], 0, 15);

  /*������*/
  pdata = &e1_params.tone_cadence3[0];
  pdata[0] = 19;
  pdata[1] = 100;
  pdata[2] = 0xf0;
  pdata[3] = 0x3c;
  pdata[4] = 0x0f;
  memset(&pdata[5], 0, 13);

  /*֤ʵ��*/
  pdata = &e1_params.tone_cadence4[0];
  pdata[0] = 16;
  pdata[1] = 100;
  pdata[2] = 0xf0;
  pdata[3] = 0x3c;
  pdata[4] = 0x0f;
  memset(&pdata[5], 0, 13);

  /*����ȴ���*/
  pdata = &e1_params.tone_cadence5[0];
  pdata[0] = 16;
  pdata[1] = 88;
  pdata[2] = 0xff;
  memset(&pdata[3], 0, 15);

  /*����*/
  pdata = &e1_params.tone_cadence6[0];
  pdata[0] = 0;
  pdata[1] = 0;
  pdata[2] = 0;
  memset(&pdata[3], 0, 15);

  /*����*/
  pdata = &e1_params.tone_cadence7[0];
  pdata[0] = 0;
  pdata[1] = 0;
  pdata[2] = 0;
  memset(&pdata[3], 0, 15);

  /*tone�������ձ�*/
  pdata = &e1_params.reason_to_tone[0];
  pdata[0] = 0;
  pdata[1] = 1;
  pdata[2] = 2;
  pdata[3] = 5;
  pdata[4] = 4;
  pdata[5] = 1;
  pdata[6] = 1;
  pdata[7] = 1;
  pdata[8] = 0;
  pdata[9] = 3;
  memset(&pdata[10], 0, 6);
}

void init_eeprom(void)
{
	//reload_eeprom();

    if (e1_params.version != E1_CARD_VERSION) {
        LOG_W("Start default param set!");
        memset(&e1_params, 0, sizeof(e1_params));
        e1_params.version = E1_CARD_VERSION;
        
        for( int i = 0; i < E1_CARDS; i++) {
            e1_params.e1_enable[i] = 0xff;
            e1_params.e1_l2_alarm_enable[i] = 0xff;
            e1_params.e1_port_type[i] = SS7_PORT;
            e1_params.isdn_port_type[i] = PRI_CPE;
            e1_params.pll_src[i] = 0x8;
            e1_params.crc4_enable[i] = CRC4_DISABLE;
            e1_params.no1_enable[i] = NO1_DISABLE;
        }
        
        e1_params.no1_enable[0] = 8;

        //init_tone_cadence();        

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

const u8_t ring_pat[MAX_RING] = {
    1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0
};

const u8_t busy_pat[MAX_BUSY] = {
    1,1,1,1,1,1,1,0,0,0,0,0,0,0
};

const u8_t confirm_pat[MAX_CONFIRM] = {
    1,1,0,0,1,1,0,0,1,1,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0
};

const u8_t holding_pat[MAX_HOLDING] = {
    0,0,0,0,1,1,0,0,0,0,
	1,1,0,0,0,0,1,1,1,1,
	2,1,1,1,1
};

const u8_t hint_pat[MAX_HINT] = {
    1,1,0,0,1,1,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0
};

const u8_t alert_pat[MAX_ALERT] = {
    1,1,1,1,0,0,0,0,0,0,
	1,1,1,1,0,0,0,0,0,0,
	1,1,1,1,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0
};

const u8_t temp_pat[MAX_TEMP] = {
    1,1,1,1,1,1,1,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0
};

const u8_t tycle_max[] = {
    MAX_RING,
    MAX_BUSY,
    MAX_TEMP,
    MAX_HINT,
    MAX_CONFIRM,
    MAX_BUSY,
    MAX_BUSY,
    MAX_BUSY,
    MAX_RING,
    MAX_HOLDING,
    MAX_TEMP,
    MAX_TEMP,
    MAX_TEMP,
    MAX_TEMP,
    MAX_TEMP
};

const u8_t *tone_pat[] = {
    ring_pat,
    busy_pat,
    temp_pat,
    hint_pat,
    confirm_pat,
    busy_pat,
    busy_pat,
    busy_pat,
    holding_pat,
    temp_pat,
    temp_pat,
    temp_pat,
    temp_pat,
    temp_pat
};


//static u8_t tone_count[MAX_E1_TIMESLOTS * E1_PORT_PER_CARD];

void tone_rt(u8_t slot)
{
    u8_t t_on_off;
    u8_t tone_id = (slot_params[slot].connect_tone_flag >> 4) & 0x7;
    
    u8_t t_count = slot_params[slot].tone_count;
    slot_params[slot].tone_count = (t_count + 1) % tycle_max[tone_id & 0xF];

    t_on_off = *(tone_pat[tone_id & 0xf] + t_count);

    if (t_on_off == 1) {
        //connect_slot(slot & 0x1f, slot >> 5, VOICE_450HZ_TONE, TONE_E1);
        ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_ACTIVE);
    } else {
        ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_INACTIVE);
    }
}
