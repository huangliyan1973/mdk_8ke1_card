/*
 * zl50020.c
 *
 *  Created on: 2020.12.10
 *      Author: hly66
 */

#include "main.h"
#include "zl50020.h"

/* FSMC_NE1 for zl50020 */
#define ZL50020_BASE (0x60000000)
#define CONN_MEM_OFFSET (0x2000)

#define ZL_DEV (struct zl50020_dev *)ZL50020_BASE

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
    while (dev->ims != 0)
        ;
    /* disable block programming */
    dev->cr &= ~(0x8);

    //ODE enable.

    //Set ISR for STI stream clk
    dev->sicr[0] = 0x4; //16.384M for DS26518
    dev->sicr[1] = 0x1; //2.048M for I2S
    dev->sicr[2] = 0x1; // 2.048M for TONE32 Module.
    dev->sicr[3] = 0x1; //2.048M for CONF Module.
    dev->sicr[4] = 0x1; //2.048M for MFC Module.

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

#define TONE_STREAM 2
#define CONF_STREAM 3
#define MFC_STREAM 4

void connect_slot(uint16_t o_ts, uint16_t o_e1, uint16_t i_ts, uint16_t i_e1)
{
    struct zl50020_dev *dev = ZL_DEV;
    uint16_t *pcml = (uint16_t *)(ZL50020_BASE + CONN_MEM_OFFSET);
    uint16_t o_slot = (o_e1 << 5) | o_ts;

    dev->cr &= 0xFC; // CONNECT MEMORY LOW read/write.

    /* i_e1 < 8: sti5, i_e1 < 16: sti6, i_e1 < 24, sti7, i_e1 < 32, sti8 */
    uint16_t i_stream = (i_e1 >> 3) + 5;
    pcml[o_slot] = (i_stream << 9) | (i_ts << 1); /* BIT 8 - BIT 1 */
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
}
