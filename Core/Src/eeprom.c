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
#include "stm32f4xx_hal_flash_ex.h"
#include "FreeRTOS.h"
#include "eeprom.h"
#include "mtp.h"
#include "ds26518.h"

#define LOG_TAG              "eeprom"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

#define   E1_CARD_VERSION   0x1

#define FLASH_WAITETIME  50000          //FLASH等待超时时间

/* Base address of the Flash sectors */ 
#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000) /* Base address of Sector 0, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000) /* Base address of Sector 1, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000) /* Base address of Sector 2, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000) /* Base address of Sector 3, 16 Kbytes   */
#define ADDR_FLASH_SECTOR_4     ((uint32_t)0x08010000) /* Base address of Sector 4, 64 Kbytes   */
#define ADDR_FLASH_SECTOR_5     ((uint32_t)0x08020000) /* Base address of Sector 5, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_6     ((uint32_t)0x08040000) /* Base address of Sector 6, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_7     ((uint32_t)0x08060000) /* Base address of Sector 7, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_8     ((uint32_t)0x08080000) /* Base address of Sector 8, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_9     ((uint32_t)0x080A0000) /* Base address of Sector 9, 128 Kbytes  */
#define ADDR_FLASH_SECTOR_10    ((uint32_t)0x080C0000) /* Base address of Sector 10, 128 Kbytes */
#define ADDR_FLASH_SECTOR_11    ((uint32_t)0x080E0000) /* Base address of Sector 11, 128 Kbytes */

#ifdef FLASH_1M
#define FLASH_USER_START_ADDR    ADDR_FLASH_SECTOR_11 	
#define FLASH_USER_SECTOR        FLASH_SECTOR_11
#else
#define FLASH_USER_START_ADDR    ADDR_FLASH_SECTOR_7	
#define FLASH_USER_SECTOR        FLASH_SECTOR_7
#endif

e1_params_t	e1_params;
ram_params_t ram_params;
slot_t  slot_params[SLOT_MAX];
u8_t  group_user[81];

void update_eeprom(void)
{
	HAL_StatusTypeDef flashStatus = HAL_OK;

    LOG_D("Start update eeprom!");

    taskENTER_CRITICAL();
    
	HAL_FLASH_Unlock();             //解锁

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP    | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |\
                         FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR| FLASH_FLAG_PGSERR);  
    
    FLASH_Erase_Sector(FLASH_USER_SECTOR,FLASH_VOLTAGE_RANGE_3);

	flashStatus=FLASH_WaitForLastOperation(FLASH_WAITETIME);
	if (flashStatus == HAL_OK){
		uint32_t *pbuf = (uint32_t *)&e1_params;
		uint16_t cn_32 = sizeof(e1_params)/4 + 1;
		for (uint16_t i = 0; i < cn_32; i++)
			HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_USER_START_ADDR+i*4, pbuf[i]);
	}

    HAL_FLASH_Lock();
    taskEXIT_CRITICAL();
    if (flashStatus == HAL_OK) {
        LOG_D("eeprom update succeed!");
    } else {
        LOG_E("eeprom update failed!");
    }    
}

void reload_eeprom(void)
{
    memcpy((void *)&e1_params, (void *)FLASH_USER_START_ADDR, sizeof(e1_params));
	//e1_params = *(e1_params_t *)FLASH_USER_START_ADDR;
}

void init_tone_cadence(void)
{
  /*回铃音*/
  u8_t *pdata = &e1_params.tone_cadence0[0];
  pdata[0] = 16;
  pdata[1] = 100;
  pdata[2] = 0xff;
  pdata[3] = 0xff;
  pdata[4] = 0xff;
  memset(&pdata[5], 0, 13);

  /*忙音*/
  pdata = &e1_params.tone_cadence1[0];
  pdata[0] = 16;
  pdata[1] = 14;
  pdata[2] = 0xfe;
  memset(&pdata[3], 0, 15);

  /*空号音*/
  pdata = &e1_params.tone_cadence2[0];
  pdata[0] = 16;
  pdata[1] = 14;
  pdata[2] = 0xfe;
  memset(&pdata[3], 0, 15);

  /*提醒音*/
  pdata = &e1_params.tone_cadence3[0];
  pdata[0] = 19;
  pdata[1] = 100;
  pdata[2] = 0xf0;
  pdata[3] = 0x3c;
  pdata[4] = 0x0f;
  memset(&pdata[5], 0, 13);

  /*证实音*/
  pdata = &e1_params.tone_cadence4[0];
  pdata[0] = 16;
  pdata[1] = 100;
  pdata[2] = 0xf0;
  pdata[3] = 0x3c;
  pdata[4] = 0x0f;
  memset(&pdata[5], 0, 13);

  /*呼入等待音*/
  pdata = &e1_params.tone_cadence5[0];
  pdata[0] = 16;
  pdata[1] = 88;
  pdata[2] = 0xff;
  memset(&pdata[3], 0, 15);

  /*保留*/
  pdata = &e1_params.tone_cadence6[0];
  pdata[0] = 0;
  pdata[1] = 0;
  pdata[2] = 0;
  memset(&pdata[3], 0, 15);

  /*保留*/
  pdata = &e1_params.tone_cadence7[0];
  pdata[0] = 0;
  pdata[1] = 0;
  pdata[2] = 0;
  memset(&pdata[3], 0, 15);

  /*tone语音对照表*/
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
	reload_eeprom();

    if (e1_params.version != E1_CARD_VERSION) {
        
        load_default_param();
    }
}

void load_default_param(void)
{
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
        memset ((void *)(&e1_params.pc_magic[i].type), 0, sizeof(e1_params.pc_magic[i]));
    }
    
    init_tone_cadence();

    memset((void *)&e1_params.reserved[0], 0, 128);
    
    e1_params.iap_value = 0;

    update_eeprom();
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
#if 0
    if (slot_params[slot].connect_time > 0){
        slot_params[slot].connect_time--;
        if (slot_params[slot].connect_time == 0) {
            ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_INACTIVE);
            //connect_tone()
            slot_params[slot].connect_tone_flag = 0xf0;
            return;
        }
    }

    if (slot_params[slot].tone_count == 1) {
        LOG_I("Slot 0x'%x' tone_id = %d, connect_time=%d", slot,tone_id, slot_params[slot].connect_time+1);
        LOG_HEX("tone_pat", 10, (u8_t *)tone_pat[tone_id & 0xf], tycle_max[tone_id & 0xf]);
    }
#endif
    if (t_on_off == 1) {
        //connect_slot(slot & 0x1f, slot >> 5, VOICE_450HZ_TONE, TONE_E1);
        ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_ACTIVE);
    } else {
        ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_INACTIVE);
    }

    
}
