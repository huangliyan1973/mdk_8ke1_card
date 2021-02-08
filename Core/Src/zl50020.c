/*
 * zl50020.c
 *
 *  Created on: 2020.12.10
 *      Author: hly66
 */

#include "main.h"
#include "zl50020.h"
#include "eeprom.h"
#include "lwip/def.h"

#define LOG_TAG              "switch"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

/* FSMC_NE1 for zl50020 */
#define ZL50020_BASE (0x60000000)
#define CONN_MEM_OFFSET (0x2000)

#define MODULE_BASE (0x6C000000)
#define TONE32_OFFSET  (0xF000)
#define CONF_OFFSET    (0xE000)
#define MFC_OFFSET     (0xD000)
#define CARD_ID_OFFSET (0xA000)
#define LED_RED_OFFSET       (0xB000)
#define LED_GREEN_OFFSET        (0xC000)

#define TONE32_ADDR             (MODULE_BASE + TONE32_OFFSET)
#define CONF_ADDR               (MODULE_BASE + CONF_OFFSET)
#define MFC_ADDR                (MODULE_BASE + MFC_OFFSET)
#define CARD_ID_ADDR            (MODULE_BASE + CARD_ID_OFFSET)
#define E1_GREEN_LED_ADDR       (MODULE_BASE + LED_GREEN_OFFSET)
#define E1_RED_LED_ADDR         (MODULE_BASE + LED_RED_OFFSET)

#define CONF_C_ADDR     (CONF_ADDR + 1)

#define ZL_DEV (struct zl50020_dev *)ZL50020_BASE

extern u8_t card_id;

void zl50020_init(void)
{
    struct zl50020_dev *dev = ZL_DEV;

    /* CR
	 * Bit 11 = 0, OPM is Divided Clock Mode.
	 * Bit 9: FPINPOS = 0, ST-BUS.
	 * Bit 8: CKINP = 0, No inversion.
	 * Bit 7: FPINP = 1, Need inversion.
	 * Bit 6-5: CKIN1 - 0 = 00, CKi=16.384M, FPi=61ns.
	 * Bit 4: VAREN = 0, Variable Delay Mode disable.
	 * Bit 3: MBPE = 0, Memory Block Programming disable.
	 * Bit 2: OSB = 1, Output enable.
	 * Bit 1-0: MS1-0, 00: Connection Memory Low Read/Write,
	 * 				   01: Connection Memory High Read/Write
	 * 				   10: Data Memory Read
	 * 				   11: Reserved
	 */
    dev->cr = 0x84;

    HAL_Delay(2);

    /* Block programming enable */
    dev->cr |= 0x8;
    /*
	 * Bit3-1: 000, BPD
	 * Bit0: MBPS = 0;
	 */
    dev->ims = 0;
    dev->ims = 1;
    //while (dev->ims != 0);
	
    /* disable block programming */
    dev->cr &= ~(0x8);

    //ODE enable.
    //PC0 for ODE function.
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);

    //Set ISR for STI stream clk
    dev->sicr[0] = 0x4; // 16.384M for DS26518
    dev->sicr[1] = 0x1; // 2.048M for I2S
    dev->sicr[2] = 0x1; // 2.048M for TONE32 Module.
    dev->sicr[3] = 0x1; // 2.048M for CONF Module.
    dev->sicr[4] = 0x1; // 2.048M for MFC Module.

    dev->sicr[5] = dev->sicr[6] = dev->sicr[7] = dev->sicr[8] = 0x4; //16.384M for OTHER 8KE1 Card.

    //Set OSR for STO stream clk
    dev->socr[0] = 0x4;
    dev->socr[1] = 0x1;
    dev->socr[2] = 0x1;
    dev->socr[3] = 0x1;

    dev->socr[5] = dev->socr[6] = dev->socr[7] = dev->socr[8] = 0x4;
}

/****************************************************************
* MAP THE CONNECTION OF 32-32 E1 SWICTH

*   O_TS  : 0 - 31 OUT E1  TIME SLOT
*   O_PCM : 0 - 7  OUT E1# (DEMUX TO E1)
*   I_TS  : 0 - 31 IN  E1 TIME SLOT
*   I_PCM : 0 - 31 IN  E1# (E1 TO MUX))
****************************************************************/

void connect_slot(uint16_t o_ts, uint16_t o_e1, uint16_t i_ts, uint16_t i_e1)
{
    struct zl50020_dev *dev = ZL_DEV;
    uint16_t *pcml = (uint16_t *)(ZL50020_BASE + CONN_MEM_OFFSET);
    uint16_t o_slot = (o_e1 << 5) | o_ts;

    dev->cr &= 0xFC; // CONNECT MEMORY LOW read/write.

    /* i_e1 < 8: sti5, i_e1 < 16: sti6, i_e1 < 24, sti7, i_e1 < 32, sti8 */
    uint16_t i_stream = (i_e1 >> 3) + 5;
    pcml[o_slot] = (i_stream << 9) | (i_ts << 1); /* BIT 8 - BIT 1 */

    LOG_I("connect slot %04x <-- %04x, connect_memory[%d]=%04x",
        o_slot, (i_e1 << 5) | i_ts, o_slot, pcml[o_slot]);
}
/* i_stream possible value:
TONE_STREAM,
CONF_STREAM,
MFC_STREAM
*/
void connect_tone(uint16_t o_ts, uint16_t o_e1, uint16_t i_ts, uint16_t i_stream)
{
    struct zl50020_dev *dev = ZL_DEV;
    uint16_t *pcml = (uint16_t *)(ZL50020_BASE + CONN_MEM_OFFSET);
    uint16_t o_slot = (o_e1 << 5) | o_ts;

    dev->cr &= 0xFC; // CONNECT MEMORY LOW read/write.

    pcml[o_slot] = (i_stream << 9) | (i_ts << 1); /* BIT 8 - BIT 1 */

    LOG_I("connect tone %04x <-- %04x/%d, connect_memory[%d]=%04x",
        o_slot, i_ts, i_stream, o_slot, pcml[o_slot]);
}

void m34116_mode(void)
{
    u8_t *control = (u8_t *)CONF_C_ADDR;
    *control = 0x19; 
}

void m34116_disconnect(u8_t slot)
{
    u8_t *data = (u8_t *)CONF_ADDR;
    *data = slot;

    *(data + 1) = 0xF;
}

void m34116_conf_connect(u8_t p, u8_t gaini, u8_t ai, u8_t gaino, u8_t ao, u8_t c, u8_t s, u8_t pt)
{
    u8_t *reg = (u8_t *)CONF_ADDR;

    c = c & 0x1F;
    
    if (s == 1) {
        p += 32;
    }
    *reg = p;

    if (gaini == 1) {
        ai += 16;
    }
    *reg = ai;

    if (gaino == 1) {
        ao += 16;
    }
    *reg = ao;

    if (pt == 1) {
        c += 64;
    }
    *reg = c;

    *(reg + 1) = 0x7;
}

u8_t m34116_status(u8_t slot)
{
    u8_t *reg = (u8_t *)CONF_ADDR;
    
    LOG_I("conf data = %x", *reg);

    *reg = slot & 0x1F;
    *(reg + 1) = 0x6;

    LOG_I("after set, conf data = %x, data1=%x", *reg, *(reg+1));
    return (*reg);
}

u8_t read_dtmf(u8_t slot)
{
    u8_t *data = (u8_t *)MFC_ADDR;
    return data[slot & 0x1F];
}

void conf_module_detect(void)
{
    m34116_mode();
    m34116_conf_connect(10, 0, 2, 0, 0, 1, 1, 0);
    if (m34116_status(1) == 10) {
        m34116_disconnect(1);
        ram_params.conf_module_installed = 1;
        LOG_I("CONF MODULE INSTALLED!");
    } else {
        ram_params.conf_module_installed = 0;
        LOG_E("CONF MODULE NOT INSTALLED!\n");
    }
}

void mfc_module_detect(void)
{
    if (read_dtmf(0) == 0 && read_dtmf(16) == 0) {
       LOG_I("MFC MODULE INSTALLED!");
        ram_params.mfc_module_installed = 1;
    } else {
        LOG_E("MFC MODULE NOT INSTALLED!\n");
        ram_params.mfc_module_installed = 0;
    }
}

u8_t get_card_id(void)
{
    u8_t *data = (u8_t *)CARD_ID_ADDR;
    return (*data);
}

void set_card_e1_led(void)
{
    u8_t *led_red = (u8_t *)E1_RED_LED_ADDR;
    u8_t *led_green = (u8_t *)E1_GREEN_LED_ADDR;

    *led_green = (!ram_params.e1_l1_alarm & e1_params.e1_enable[card_id & 0xF]) & e1_params.e1_l2_alarm_enable[card_id & 0xF];
    *led_red = (ram_params.e1_l1_alarm | !ram_params.e1_l2_alarm) & e1_params.e1_enable[card_id & 0xF];
}

void test_e1_led(u8_t red, u8_t green)
{
	u8_t *led_red = (u8_t *)E1_RED_LED_ADDR;
    u8_t *led_green = (u8_t *)E1_GREEN_LED_ADDR;
	
	*led_red = red;
	*led_green = green;
}

void zl50020_test(void)
{
    struct zl50020_dev *dev = ZL_DEV;

    zl50020_init();

    u16_t data = dev->sicr[0];
    if (dev->sicr[0] != 0x4) {
        goto fault;
    }

    if (dev->sicr[1] != 0x1) {
        goto fault;
    }

    if (dev->socr[0] != 0x4) {
        goto fault;
    }

    if (dev->socr[1] != 0x1) {
        goto fault;
    }

    dev->ocfcr = 0x1cf;

    if (dev->ocfcr != 0x1cf) {
        goto fault;
    }
    LOG_I("ZL50020 TEST OK!");
	return;

fault:
    LOG_E("ZL50020 TEST FAILED!");
}

void led_test(void)
{
	u8_t *led_green = (u8_t *)E1_GREEN_LED_ADDR;
	u8_t *led_red = (u8_t *)E1_RED_LED_ADDR;
	
    LED1_GREEN_ON;
    LED2_GREEN_ON;
    HAL_Delay(1000);
    LED1_RED_ON;
    LED2_RED_ON;
    HAL_Delay(1000);
    LED1_ORG_ON;
    LED2_ORG_ON;
    HAL_Delay(1000);
    LED1_OFF;
    LED2_OFF;
	
	HAL_Delay(1000);
	*led_green = 0xff;
	*led_red = 0;
    HAL_Delay(1000);
    *led_green = 0x0;
	*led_red = 0xff;
    HAL_Delay(1000);
    *led_green = 0xff;
	*led_red = 0xff;
	HAL_Delay(1000);
    *led_green = 0;
	*led_red = 0;
}

void module_test(void)
{
    conf_module_detect();

    mfc_module_detect();
}
