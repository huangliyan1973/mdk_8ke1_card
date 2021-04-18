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
#include "ds26518.h"
#include "server_interface.h"

#define LOG_TAG              "switch"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

/* FSMC_NE1 for zl50020 */
#define ZL50020_BASE (0x60000000)
#define CONN_MEM_OFFSET (0x4000)

#define CML_STO_ADDR(o_stream) (ZL50020_BASE + CONN_MEM_OFFSET + 0x200*o_stream)

#define ZL50020_CML_ADDR (ZL50020_BASE + CONN_MEM_OFFSET)

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

#define ZL_DEV (struct zl50020_dev *)ZL50020_BASE
#define ZL_CML (struct zl50020_cml *)ZL50020_CML_ADDR
/** FOR V3.61 
#define PCM_BUS_START_STREAM         16
#define MODULE_START_STREAM         24
#define TONE_START_SLOT         0
#define CONF_START_SLOT         32
#define MFC_START_SLOT          64
**/

/* FOR V3.1 */
#define PCM_BUS_START_STREAM         12
#define MODULE_START_STREAM         13

#define TONE_START_SLOT             96
#define CONF_START_SLOT             64
#define MFC_START_SLOT              96


//#define ZL50020_16M_CONNECTION

extern u8_t card_id;

static int zl50020_inited = 0;

void zl50020_connect_memory_init(void)
{
    struct zl50020_cml *cml = ZL_CML;
    u8_t start_stream = PCM_BUS_START_STREAM;
    u8_t o_stream, o_ts;

    for(int i = 0; i < E1_PORT_PER_CARD; i++) {
        for(int slot = 0; slot < MAX_E1_TIMESLOTS; slot++) {
            o_stream = start_stream + i/4;
            o_ts = ((i << 5) + slot) & 0x7f;
            cml->sto_connect[o_stream][o_ts] = (i << 9) | (slot << 1);
            /***
            LOG_I("con: 0x%x[%x] <-- 0x%x[%x]\t[%p] = %x", 
                o_ts, o_stream, slot, i,
                &cml->sto_connect[o_stream][o_ts], 
                cml->sto_connect[o_stream][o_ts]);
            **/
        }
        //LOG_D("--------------------------------------------------------");
    }

    LOG_D("------------------------------TONE------------------------");
    if ((card_id & 0x0f) == 0) {
        for (int i = 0; i < MAX_E1_TIMESLOTS; i++) {
            cml->sto_connect[MODULE_START_STREAM][i + TONE_START_SLOT] = (TONE_STREAM << 9) | (i << 1);
            /**
            LOG_I("con: 0x%x[%x] <-- 0x%x[%x] [%p] = %x", 
                i + TONE_START_SLOT, MODULE_START_STREAM, i, TONE_STREAM,
                &cml->sto_connect[MODULE_START_STREAM][i + TONE_START_SLOT], 
                cml->sto_connect[MODULE_START_STREAM][i + TONE_START_SLOT]);
            **/
        }        
    }

    LOG_D("------------------------------CONF-------------------------");
    if (ram_params.conf_module_installed) {
        for (int i = 0; i < MAX_E1_TIMESLOTS; i++) {
            cml->sto_connect[MODULE_START_STREAM][i+CONF_START_SLOT] = (CONF_STREAM << 9) | (i << 1);
            /***
            LOG_I("con: 0x%x[%x] <-- 0x%x[%x] [%p] = %x", 
                i + CONF_START_SLOT, MODULE_START_STREAM, i, CONF_STREAM,
                &cml->sto_connect[MODULE_START_STREAM][i + CONF_START_SLOT], 
                cml->sto_connect[MODULE_START_STREAM][i + CONF_START_SLOT]);
            **/
        }
    }

}

void zl50020_connect_memory_16m_init(void)
{
    struct zl50020_cml *cml = ZL_CML;
    u8_t start_stream = (card_id & 0xf) + PCM_BUS_START_STREAM;
    u8_t o_stream, o_ts;

    for (int i = 0; i < E1_PORT_PER_CARD; i++) {
        for (int slot = 0; slot < MAX_E1_TIMESLOTS; slot++) {
            o_stream = start_stream;
            o_ts = (i << 5) + slot;
            cml->sto_connect[o_stream][o_ts] = (i << 9) | (slot << 1);
            LOG_I("con: 0x%x[%x] <-- 0x%x[%x] [%p] = %x", 
                o_ts, o_stream, slot, i,
                &cml->sto_connect[o_stream][o_ts], 
                cml->sto_connect[o_stream][o_ts]);
        }
        LOG_D("----------------------------------------");
    }

    LOG_D("-----------------TONE-------------------------");
    if ((card_id & 0xf) == 0) {
        for (int i = 0; i < MAX_E1_TIMESLOTS; i++) {
            cml->sto_connect[MODULE_START_STREAM][i] = (TONE_STREAM << 9) | (i << 1);
            LOG_I("con: 0x%x[%x] <-- 0x%x[%x] [%p] = %x", 
                i, MODULE_START_STREAM, i, TONE_STREAM,
                &cml->sto_connect[MODULE_START_STREAM][i], 
                cml->sto_connect[MODULE_START_STREAM][i]);
        }        
    }

    LOG_D("-----------------CONF-------------------------");
    if (ram_params.conf_module_installed) {
        for (int i = 0; i < MAX_E1_TIMESLOTS; i++) {
            cml->sto_connect[MODULE_START_STREAM][i+32] = (CONF_STREAM << 9) | (i << 1);
        }
    }

    LOG_D("-----------------MFC-------------------------");
    if (ram_params.mfc_module_installed) {
        for (int i = 0; i < MAX_E1_TIMESLOTS; i++) {
            cml->sto_connect[MODULE_START_STREAM][i+64] = (MFC_STREAM << 9) | (i << 1);
            LOG_I("con: 0x%x[%x] <-- 0x%x[%x] [%p] = %x", 
                i+64, MODULE_START_STREAM, i, TONE_STREAM,
                &cml->sto_connect[MODULE_START_STREAM][i+64], 
                cml->sto_connect[MODULE_START_STREAM][i+64]);
        }
    }
}

void zl50020_init(void)
{
    struct zl50020_dev *dev = ZL_DEV;
    u16_t ims_value;

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
    * Bit 8: = 1: the pull-down resistors on all STio pads will be enabled
     * Bit 7 = 1: STi16-31 tied low internally, STio16-31 are bi-directiona
	 * Bit3-1: 000, BPD
	 * Bit0: MBPS = 0;
	 */
    if (!(card_id & 0xf)) {
        ims_value = 0x0;
    } else {
        ims_value = 0x0;
    }
    dev->ims = ims_value; 
    HAL_Delay(2);
    dev->ims = ims_value | 1;

    HAL_Delay(2);
    LOG_W("dev->ims=%x", dev->ims);
    while (dev->ims != ims_value);
	
    /* disable block programming */
    dev->cr &= ~(0x8);

    //ODE enable.
    //PC0 for ODE function.
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_SET);

    //Set ISR for STI stream clk
    for(int i = 0; i < E1_PORT_PER_CARD; i++)
    {
        dev->sicr[i] = 0x1; //2.048M for E1 port;
        dev->socr[i] = 0x1;
    }

    dev->sicr[8] = dev->socr[8] = 0x1; // 2.048M for TONE32 Module.
    dev->sicr[9] = dev->socr[9] = 0x1; // 2.048M for MFC Module.
    dev->sicr[10] = dev->socr[10] = 0x1; // 2.048M for CONF Module.



#ifdef ZL50020_16M_CONNECTION
    for (int i = 16; i < 20; i++) {
        dev->sicr[i] = 0x4;
        dev->socr[i] = 0x4;
    }
    dev->sicr[MODULE_START_STREAM] = 0X4;
    dev->socr[MODULE_START_STREAM] = 0x4;

    zl50020_connect_memory_16m_init();
#else
    /* STio12 - STio19 for Cards exchange, 8.096M PCM 
     * STio24 for module slot, 0-31 Tone, 32-63 conf, 64 - 95 mfc_in 
    */
    for(int i = PCM_BUS_START_STREAM; i < PCM_BUS_START_STREAM + 8; i++) {
        dev->sicr[i] = 0x3;
        dev->socr[i] = 0x3;
    }
    
    zl50020_connect_memory_init();
#endif 

    LOG_D("ZL500020 REGISTER:");
    LOG_D("CR =%X, IMS=%x, SRR=%X, OCFCR=%X",dev->cr, dev->ims, dev->srr, dev->ocfcr);
    LOG_D("SICR1 = %X, SOCR1=%X, SICR12=%X, SOCR12=%X",
        dev->sicr[0], dev->socr[0], dev->sicr[PCM_BUS_START_STREAM], dev->socr[PCM_BUS_START_STREAM]);
    
    
    LOG_I("ZL50020 Init finised!, IFR=%x", dev->ifr);
    if (dev->ifr != 0) {
        LOG_E("ZL50020 Init failed cause of maximum capacity.");
    }

    zl50020_inited = 1;
    
}

void connect_bert_slot(u8_t e1_no, u8_t slot)
{
    struct zl50020_dev *dev = ZL_DEV;
    uint16_t *pcml = (uint16_t *)(ZL50020_BASE + CONN_MEM_OFFSET);
    
    dev->cr &= 0xFC;
    
    u16_t index = ((u16_t)e1_no << 5) | (slot & 0x1f);
    u16_t value = ((u16_t)(e1_no & 7) << 5) | slot;
    pcml[index] = value << 1;
    
    LOG_W("connect slot: %04x[%2x]<-- %04x[%02x], CML[%x] = %04x", index, 0, value, 0, index, pcml[index]);
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
    struct zl50020_cml *cml = ZL_CML;

    uint16_t i_stream, i_slot, conn_value;
    uint16_t o_slot, o_stream;

    //LOG_D("Connect slot o_ts[o_e1] = %x[%x] , i_ts[i_e1]=%x[%x]", o_ts,o_e1, i_ts,i_e1);
    if (i_e1 == TONE_E1) {
        /* Listen tone */
        i_stream = MODULE_START_STREAM;
        i_slot = TONE_START_SLOT + i_ts;
    } else if (i_e1 == CONF_E1) {        
        i_stream = MODULE_START_STREAM;
        i_slot = CONF_START_SLOT + i_ts;
    } else {
#ifdef ZL50020_16M_CONNECTION
        i_stream = (i_e1 >> 3) + PCM_BUS_START_STREAM;
        i_slot = ((i_e1 & 0x7) << 5) + i_ts;
#else
        /* card_id = i_e1 >> 3;
         *  stream = card_id * 2 + PCM_BUS_START_STREAM
        */
        i_stream = (i_e1 >> 2) + PCM_BUS_START_STREAM;
        i_slot = ((i_e1 & 0x3) << 5) + i_ts;
#endif
    }

    if (o_e1 == TONE_E1 && ram_params.mfc_module_installed == 1) { 
        /* MFC Decode */
        o_stream = MFC_STREAM;
        o_slot = o_ts;
    } else if (o_e1 == CONF_E1 && ram_params.conf_module_installed == 1) {
        o_stream = CONF_STREAM;
        o_slot = o_ts;
    } else {
        o_stream = o_e1;
        o_slot = o_ts;
    }
    
    conn_value = (i_stream << 9) | (i_slot << 1);
  
    dev->cr &= 0xFC; // CONNECT MEMORY LOW read/write.

    cml->sto_connect[o_stream][o_slot] = conn_value;

    LOG_I("Connect slot 0x%x[%x] <-- 0x%x[%x]\t[%p]=0x%x", o_slot, o_stream, i_slot, i_stream,
        &cml->sto_connect[o_stream][o_slot], cml->sto_connect[o_stream][o_slot]);

}

/* i_stream possible value:
TONE_STREAM,
CONF_STREAM,
MFC_STREAM
*/
void connect_tone(uint16_t o_ts, uint16_t o_e1, uint16_t i_ts)
{
    struct zl50020_dev *dev = ZL_DEV;
    struct zl50020_cml *cml = ZL_CML;
    
    dev->cr &= 0xFC; 

    //pcml[o_slot] = (i_stream << 9) | (i_ts << 1); /* BIT 8 - BIT 1 */
    cml->sto_connect[o_e1][o_ts] = (MODULE_START_STREAM << 9) | ((i_ts + TONE_START_SLOT) << 1);

    LOG_I("connect tone 0x%x[%x] <-- 0x%x[%x]",
        o_ts, o_e1, i_ts + TONE_START_SLOT, MODULE_START_STREAM);
}

void zl50020_bitDelay(u8_t stream_no, u8_t bit_delay)
{
    struct zl50020_dev *dev = ZL_DEV;
    dev->sicr[stream_no & 0x1f] |= ((bit_delay & 7) << 6);
    LOG_I("stream '%d' set bitdelay=%d, sicr value=0x%x", stream_no, bit_delay, dev->sicr[stream_no & 0x1f]);
}

void zl50020_bitAdvancement(u8_t stream_no, u8_t bit_adv)
{
    struct zl50020_dev *dev = ZL_DEV;
    dev->socr[stream_no & 0x1f] |= ((bit_adv & 7) << 4);
    LOG_I("stream '%d' set bitadv=%d, socr value=0x%x", stream_no, bit_adv, dev->socr[stream_no & 0x1f]);
}

void zl50020_frac_bit_adv(u8_t stream_no)
{
    struct zl50020_dev *dev = ZL_DEV;
    dev->socr[stream_no & 0x1f] |= 0x80; // 2/4 bit for advancement.
    LOG_I("stream '%d' socr value=0x%x", stream_no, dev->socr[stream_no & 0x1f]);
}

void print_zl50020_register(void)
{
    uint16_t *reg = (uint16_t *)ZL50020_BASE;

    LOG_D("CR=%04x, IMS=%04x, SRR=%04x", *reg, *(reg + 1), *(reg+2));
    LOG_W("IFR=%x, BER Error Flag:%x/%x, BER Lock Reg: %x/%x", *(reg + 0x10), *(reg + 0x11), 
            *(reg + 0x12), *(reg + 0x13), *(reg + 0x14));
    for (int i = 0; i < 32; i++) {
        LOG_D("SICR%d = %x, SOCR%d = %x, SIQFR%d = %x", i, *(reg + 0x100 + i),
                                                        i, *(reg + 0x200 + i),
                                                        i, *(reg + 0x120 + i));
    }
}

void print_zl50020_CML(u16_t stream_no)
{
    u16_t *cml = (u16_t *)CML_STO_ADDR(stream_no);

    LOG_W("print STO%d connect memory", stream_no);
    for(int i = 0; i < 32; i=i+8) {
        LOG_D("slot %d: [%p]=%x [%p]%x [%p]%x [%p]%x [%p]%x [%p]%x [%p]%x [%p]%x",
        i, cml + i, *(cml + i), cml + i + 1, *(cml + i + 1), cml + i + 2, *(cml + i + 2), cml + i + 3, *(cml + i + 3),
        cml + i + 4, *(cml + i + 4), cml + i + 5, *(cml + i + 5), cml + i + 6, *(cml + i + 6), cml + i + 7, *(cml + i + 7));
    }
}

void print_zl50020_CML2(u16_t stream_no)
{
    struct zl50020_cml *cml = ZL_CML;

    LOG_W("print STO%d connect memory", stream_no);
    for(int i = 0; i < 32; i=i+8) {
        LOG_D("slot %d: [%p]=%x [%p]%x [%p]%x [%p]%x [%p]%x [%p]%x [%p]%x [%p]%x",
        i, &cml->sto_connect[stream_no][i], cml->sto_connect[stream_no][i],
        &cml->sto_connect[stream_no][i+1], cml->sto_connect[stream_no][i+1],
        &cml->sto_connect[stream_no][i+2], cml->sto_connect[stream_no][i+2],
        &cml->sto_connect[stream_no][i+3], cml->sto_connect[stream_no][i+3],
        &cml->sto_connect[stream_no][i+4], cml->sto_connect[stream_no][i+4],
        &cml->sto_connect[stream_no][i+5], cml->sto_connect[stream_no][i+5],
        &cml->sto_connect[stream_no][i+6], cml->sto_connect[stream_no][i+6],
        &cml->sto_connect[stream_no][i+7], cml->sto_connect[stream_no][i+7]);
    }
}

void print_zl50020_cml_value(u16_t stream_no, u16_t slot)
{
    struct zl50020_cml *cml = ZL_CML;
    
    LOG_W("stream '%d' slot '%d', [%p] = %x", stream_no, slot, 
    &cml->sto_connect[stream_no][slot], cml->sto_connect[stream_no][slot]);
}

void print_zl50020(u8_t stream_no, u8_t slot)
{
    //print_zl50020_register();
    
    //print_zl50020_CML(1);
    //print_zl50020_CML(2);
    //print_zl50020_CML(3);
    //print_zl50020_CML(4);
    //print_zl50020_CML(5);
    //print_zl50020_CML(6);
    //print_zl50020_CML(7);
    //print_zl50020_CML(8);

    
    struct zl50020_cml *cml = ZL_CML;
    struct zl50020_dev *dev = ZL_DEV;


    dev->cr = 0x84;
    cml->sto_connect[stream_no][slot] = (TONE_STREAM << 9) | (0 << 1);

    if (stream_no == 0) {
        //set_ds26518_loopback(0, FRAME_LOCAL_LP);
    }
    
    print_zl50020_cml_value(stream_no, slot);
    LOG_I("Now print '%d' stream '%d' slot data memory!", stream_no, slot);

    u8_t data[200] = {0};
    u8_t dummy;

    dev->cr = 0x86; // data memory read.
    for(int i = 0; i < 200; i++) {
        data[i] = cml->sto_connect[stream_no][slot];
        for (int j = 0; j < 20*16; j++) {
            dummy = dev->cr;
            dev->cr = 0x86;
            if (dummy != 0x86) {
                LOG_E("Error!");
            }
        }
    }
    dev->cr = 0x84;
    LOG_HEX("", 16, data, 200);
}

void read_zl50020_data_mem(u8_t stream_no, u8_t slot, u8_t test_value)
{
    struct zl50020_cml *cml = ZL_CML;
    struct zl50020_dev *dev = ZL_DEV;
    u8_t data[300] = {0};
    u8_t dummy;
    u16_t err_count = 0;

    LOG_I("Now print '%d' stream '%d' slot data memory!", stream_no, slot);
    dev->cr = 0x86; // data memory read.
    for(int i = 0; i < 300; i++) {
        data[i] = cml->sto_connect[stream_no][slot];
        for (int j = 0; j < 20*16; j++) {
            dummy = dev->cr;
            dev->cr = 0x86;
            if (dummy != 0x86) {
                LOG_W(" Read Too faster !");
            }
        }
        if (data[i] != test_value) {
            err_count++;
        }
    }
    dev->cr = 0x84;
    if (err_count < 5) {
        LOG_W("Read ZL50020 Data value is ok for test_value : %x, errs=%d", test_value, err_count);
    }else {
        LOG_E("Read ZL50020 Data value has too err, count = %d", err_count);
        restart_system();
    }
    LOG_HEX("", 16, data, 300);
}

void set_output_clock(uint16_t ocfcr, uint16_t ocfsr)
{
    struct zl50020_dev *dev = ZL_DEV;
    /* OCFCR
     * Bit 8: FPo_OFF2 Enable
     * Bit 7: FPo_OFF1 Enable
     * Bit 6: FPo_OFF0 Enable
     * Bit 5-4: Must be 0
     * Bit 3: CKo3 and FPo3 Enable
     * Bit 2: CKo2 and FPo2 Enable
     * Bit 1: CKo1 and FPo1 Enable
     * Bit 0: CKo0 and FPo0 Enable
    */
    dev->ocfcr = ocfcr;

    /* OCFSR
    * Bit 13-12: 00 -- FPo3: 244ns, CKo3: 4.096M
    *            01 -- FPo3: 122ns, CKo3: 8.192M
    *            02 -- FPo3: 61ns,  CKo3: 16.384M
    *            03 -- FPo3: 30ns,  CKo3: 32.768M
    * Bit 11: Output Clock (CKo3) Polarity Selection. 0: Falling edge aligns with frame boundary. 1: rising edge.
    * Bit 10: Output Frame Pulse (FPo3) Polarity Selection. 0:negative frame pulse format. 1: positive
    * Bit 9:  Output Frame Pulse (FPo3) Position. 0: ST-BUS, 1: GCI-BUS
    * Bit 8:  Output Clock (CKo2) Polarity Selection.
    * Bit 7:  Output Frame Pulse (FPo2) Polarity Selection.
    * Bit 6:  Output Frame Pulse (FPo2) Position.
    * Bit 5:  Output Clock (CKo1) Polarity Selection.
    * Bit 4:  Output Frame Pulse (FPo1) Polarity Selection.
    * Bit 3:  Output Frame Pulse (FPo1) Position.
    * Bit 2:  Output Clock (CKo0) Polarity Selection.
    * Bit 1:  Output Frame Pulse (FPo0) Polarity Selection.
    * Bit 0:  Output Frame Pulse (FPo0) Position.
    */
    dev->ocfsr = ocfsr;
}

void zl50020_clkout_test(void)
{
    /* enable CKo0 and FPo0 */
    set_output_clock(0x1, 0);
    HAL_Delay(1000); // for wave check.
    set_output_clock(0x1, 0x6); // CKo0 rising edge. FPo0: positive.
    HAL_Delay(1000);

    /* enable Cko3 and FPo3 */
    set_output_clock(0x8, 0); // 244ns FPo3, CKo3: 4M
    HAL_Delay(1000);
    set_output_clock(0x8, 0x1000); // 122ns, 8M
    HAL_Delay(1000);
    set_output_clock(0x8, 0x2000); // 61ns, 16M
    HAL_Delay(1000);
    set_output_clock(0x8, 0x3000); // 30ns, 32M
    HAL_Delay(1000);
    set_output_clock(0x8, 0xC00); // 244ns, 4M, rising edge, positive.
    
    set_output_clock(0x1cf,0);
    
}

void send_msg(u16_t stream_no, u16_t slot, u8_t data)
{
    struct zl50020_dev *dev = ZL_DEV;
    struct zl50020_cml *cml = ZL_CML;
    //u16_t *pcml = (u16_t *)CML_STO_ADDR(stream_no);

    /*First set ds26518 monitor slot */
    ds26518_monitor_tx_slot(slot >> 5, slot & 0x1f);
    ds26518_monitor_rx_slot(slot >> 5, slot & 0x1f);
    //set_ds26518_loopback(slot >> 5, FRAME_LOCAL_LP);
    ds26518_port_init(slot >> 5, CCS_TYPE);
    ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_ACTIVE);

    dev->cr &= 0xFC;
    
    cml->sto_connect[stream_no][slot] = ((u16_t)(data << 3)) | 0x3;

    //HAL_Delay(1);
    LOG_W("Now, monitor e1=%d, slot=%d, data=%x", slot >> 5, slot & 0x1f, data);
    LOG_W("IFR=%x, BER Error Flag:%x/%x, BER Lock Reg: %x/%x", dev->ifr, dev->berfr0, dev->berfr1, dev->berlr0, dev->berlr1);
    for (int i = 0; i < 10; i++) {
        u8_t send_data = data++;
        cml->sto_connect[stream_no][slot] = (send_data << 3) | 0x3;
        u8_t tx_data = ds26518_read_monitor_tx_slot(slot >> 5);
        u8_t rx_data = ds26518_read_monitor_rx_slot(slot >> 5);
        LOG_D("CML addr:%p, value=%x", &cml->sto_connect[stream_no][slot], cml->sto_connect[stream_no][slot]);
        LOG_D("message data = %x, tx_data=%x, rx_data=%x", send_data, tx_data, rx_data);
    }
    
}

void enable_prbs_function(u8_t stream_no) 
{
    struct zl50020_dev *dev = ZL_DEV;
    struct zl50020_cml *cml = ZL_CML;

    dev->ims |= PRBS_Receiver_Enable | PRBS_Transmitter_Enable;

    dev->cr &= 0xFC; //CML enable

    /* enable sto5 for BER transmiter, 0-255 slot . */
    /*
    for (int i = 0; i < 256; i++) {
        pcml[i] = (0x10 << 1) | 1;
    }
    */
    ds26518_monitor_tx_slot(0, 1);
    ds26518_monitor_rx_slot(0, 1);
    ds26518_port_init(0, CCS_TYPE);
    //set_ds26518_loopback(0, FRAME_LOCAL_LP);
    ds26518_e1_slot_enable(0, 1, VOICE_ACTIVE);

    cml->sto_connect[stream_no][1] = 0x5;

    LOG_D("CML 1 = %d", cml->sto_connect[stream_no][1]);
    /* enable Sti5 for Ber receiver. */
    dev->brlr[stream_no] = 1;
    dev->brsr[stream_no] = 1;
    dev->brcr[stream_no] = Bit_Error_Rate_Counter_Clear;
    HAL_Delay(1); // must delay 250us or more.
    LOG_D("error count = %d", dev->brer[stream_no]);
    
    dev->brcr[stream_no] = Bit_Error_Rate_Test_Start;
    LOG_D("BRCR = %x", dev->brcr[stream_no]);
}

void print_prbs_value(u8_t stream_no, int stop_flag)
{
    struct zl50020_dev *dev = ZL_DEV;

    if (stop_flag) {
        LOG_W("Stop Test! BERT Test stream'%d': error counts=%d", stream_no, dev->brer[stream_no]);
        dev->ims &= ~(PRBS_Receiver_Enable | PRBS_Transmitter_Enable);
        dev->brcr[stream_no] = 0;
        
    } else {
        LOG_I("BERT Test stream'%d': error counts=%d", stream_no, dev->brer[stream_no]);

    }
}

void test_prbs(void)
{
/* 
    enable_prbs_function(0);
    ds26518_monitor_test(0, 1);
    print_prbs_value(0, 0);
    ds26518_monitor_test(0, 1);
    print_prbs_value(0, 0);
    ds26518_monitor_test(0, 1);
    print_prbs_value(0, 0);
*/   
}

#define CONF_C_ADDR     (CONF_ADDR + 1)
#define CONF_DATA       *((u8_t *)(CONF_ADDR))
#define CONF_CONTROL    *((u8_t *)CONF_C_ADDR)

static void delay_us(int c)
{
    if (c < 0)
        return;
    while(c) {
        for(int i = 0; i < c; i++) {
            c--;
            c++;
        }
        c--;
    }
}

u8_t m34116_status(u8_t slot)
{
    CONF_DATA = slot & 0x1F;
    delay_us(100);
    CONF_CONTROL = 0x6;
    delay_us(100);
    
    return CONF_DATA;
}

u8_t m34116_other_status(void)
{
    delay_us(100);
    return CONF_DATA;
}

void m34116_mode(void)
{
    CONF_CONTROL = 0x19; 
    HAL_Delay(2);
}

void m34116_disconnect(u8_t slot)
{
    //delay_us(100);
    CONF_DATA = slot & 0x1f;
    delay_us(100);
    CONF_CONTROL = 0xf;
    delay_us(100);

    //LOG_I("m34116 '%x' slot dismiss conference, register value=%x", slot, m34116_status(slot));

}

void m34116_conf_connect(u8_t p, u8_t gaini, u8_t ai, u8_t gaino, u8_t ao, u8_t c, u8_t s, u8_t pt)
{
    u8_t conf_no;
    //u8_t status0, status1, status2;
    
    delay_us(100);
    c = c & 0x1F;
    
    if (s == 1) {   /* P Conf number */
        p += 32;
    }
    CONF_DATA = p;
    delay_us(100);

    if (gaini == 1) { /* AI input Gain and attenuation */
        ai += 16;
    }
  
    CONF_DATA = ai;
    delay_us(100);

    if (gaino == 1) { /* AO output gain and attenuation */
        ao += 16;
    }
   
    CONF_DATA = ao;
    delay_us(100);

    if (pt == 1) {  /* C channel number */
        c += 64;
    }

    CONF_DATA = c;

    delay_us(100);
    CONF_CONTROL = 0x7;
    
    conf_no = (s == 1 ? p-32 : p);
    LOG_I("Conference set, Conf_no:%d, First member:%s, Ai: %x, Ao: %x, channle:%x",
        conf_no, s==1?"Yes":"No", ai, ao, c);

#if 0
    status0 = m34116_status(c & 0x1f);
    status1 = m34116_other_status();
    status2 = m34116_other_status();
    LOG_I("Read m34116 register status, 0x'%x' slot, conf_no=%d, ai=%d, ao=%d", c, status0, status1, status2);
    if (status0 != conf_no || status1 != ai || status2 != ao) {
        LOG_W("write error!");
    }
#endif
}

void m34116_transparent_mode(u8_t slot)
{
    CONF_DATA = 0; /* ai = 0*/
    delay_us(100);
    CONF_DATA = 3; /* ao = 0 */
    delay_us(100);
    CONF_DATA = slot;
    delay_us(100);
    CONF_CONTROL = 0x3;
}

u8_t m34116_check_overlow(void)
{
    delay_us(10);
    CONF_CONTROL = 0xa;
    delay_us(10);
    return CONF_DATA;
}

void m34116_transparent_test(u8_t slot)
{
    u8_t data0, data1, data2;
    m34116_mode();
    m34116_transparent_mode(slot);
  
    HAL_Delay(1);
    data0 = m34116_status(slot);
   
    data1 = m34116_other_status();
    data2 = m34116_other_status();

    LOG_I("m34116 set transparant mode, check:%s, data0=%d, ai=%d, ao=%d",
    data0 == 31 ? "YES":"NO", data0, data1, data2);
}

void m34116_conference_test(u8_t slot)
{
    u8_t data0, data1, data2;
    u8_t conf_no;
    m34116_mode();
    
    conf_no = 10;
    m34116_conf_connect(conf_no, 0, 1, 0, 3, slot, 1, 0);
    data0 = m34116_status(slot);
    data1 = m34116_other_status();
    data2 = m34116_other_status();
    
    LOG_I("m34116 set conference mode, '%d' slot conference_no:%s, data0=%d, ai=%d, ao=%d",
    data0 == conf_no ? "YES":"NO", slot, data0, data1, data2);
    
    LOG_D("add '%d' slot in conference", slot+1);
    m34116_conf_connect(conf_no, 0, 2, 0, 2, slot+1, 0, 0);
    data0 = m34116_status(slot+1);
    data1 = m34116_other_status();
    data2 = m34116_other_status();
    
    LOG_I("'%d' slot conference_no:%s, data0=%d, ai=%d, ao=%d",
    slot + 1, data0 == conf_no ? "YES":"NO", data0, data1, data2);
}

u8_t read_dtmf(u8_t slot)
{
    u8_t *data = (u8_t *)MFC_ADDR;
    return data[slot & 0x1F];
}

static void dismiss_all_conf(void)
{
    for (int i = 0; i < MAX_E1_TIMESLOTS; i++)
    {
        m34116_disconnect(i);
    }
}

void conf_module_detect(void)
{
    m34116_mode();
    m34116_conf_connect(10, 0, 2, 0, 0, 1, 1, 0);
    if (m34116_status(1) == 10) {
        m34116_disconnect(1);
        ram_params.conf_module_installed = 1;
        dismiss_all_conf();
        LOG_I("CONF MODULE INSTALLED!");
    } else {
        ram_params.conf_module_installed = 0;
        LOG_W("CONF MODULE NOT INSTALLED!\n");
    }
}

void m34116_zl50020_test(u8_t slot)
{
    struct zl50020_cml *cml = ZL_CML;
    u8_t idle_code = 0x55;
    u8_t test_e1 = 0;
    u8_t test_slot = slot;

    if (!zl50020_inited) {
        zl50020_init();
    }
   
    LOG_I("Now test ZL50020 and DS26518 slot connection, test_slot = %d, test_e1=%d, idle_code=%x",
        test_slot, test_e1, idle_code);
    ds26518_set_idle_code(test_e1,test_slot, idle_code, 1);
    
    ds26518_set_idle_code(test_e1,test_slot+1, idle_code, 1);

    set_ds26518_loopback(0, FRAME_LOCAL_LP);
    LOG_D("After Loopback on e1, e1 tx ==> rx, so, read zl50020 data memory , should be=%x", idle_code);

    u8_t o_stream = (card_id & 0xf)*2 + PCM_BUS_START_STREAM;

    LOG_I("First test local stream, stream no = %d, slot=%d", test_e1, test_slot);
    HAL_Delay(2);
    read_zl50020_data_mem(test_e1, test_slot, idle_code);
    LOG_I("Then test PCM bus, stream no = %d, slot = %d", o_stream, test_slot);
    HAL_Delay(2);
    read_zl50020_data_mem(o_stream, test_slot, idle_code);

    LOG_I("Now connect conf module hear slot, and conf module set transparent mode.");
    cml->sto_connect[CONF_STREAM][slot] = (test_e1 << 9) | (test_slot << 1);
    m34116_conference_test(slot);

    LOG_I("Now read %d stream %d slot data", CONF_STREAM, slot);
    HAL_Delay(10);
    read_zl50020_data_mem(CONF_STREAM, slot, idle_code);
    read_zl50020_data_mem(CONF_STREAM,slot+1,idle_code);
    read_zl50020_data_mem(CONF_STREAM,slot+2,idle_code);
    
    /*Close test, and reset idle code and loopback */
    m34116_disconnect(slot);
    m34116_disconnect(slot+1);
    ds26518_set_idle_code(test_e1,test_slot, 0x37,0);
    set_ds26518_loopback(0, NO_LP);    
}

u8_t mfc_t32_zl50020_test(u8_t test_value)
{
    struct zl50020_cml *cml = ZL_CML;
    int test_ok = 1;
    
    /* t32负责放A1-A7， mfc负责解码，zl50020负责连接时隙 */
    
    if (!zl50020_inited) {
        zl50020_init();
    }

    cml->sto_connect[MFC_STREAM][0] = (MODULE_START_STREAM << 9) | (((test_value & 0xf) + TONE_START_SLOT) << 1);
    HAL_Delay(100);
    u8_t read_value = read_dtmf(0);
    LOG_I("write=%x\tread=%x", test_value, read_value);
    if (test_value != (read_value & 0xf)) {
        LOG_E("MFC decode ERROR on %x-->%x", test_value, read_value);
        test_ok = 0;
    }
   
    connect_slot(0, MFC_STREAM, TONE_SILENT,TONE_E1);

    return test_ok;   
}

void mfc_module_detect(void)
{
    u8_t mfc_installed = 0;

    if (read_dtmf(0) == 0 && read_dtmf(16) == 0 && read_dtmf(2) == 0) {
        for (int i = 0; i < 16; i++) {
            mfc_installed = mfc_t32_zl50020_test(i);
            
            if (mfc_installed == 0) {
                LOG_W("MFC MODULE NOT INSTALLED!\n");
                ram_params.mfc_module_installed = 0;
                return;
            }
            
        }
        LOG_I("MFC MODULE INSTALLED!");
        ram_params.mfc_module_installed = 1;

    } else {
        LOG_W("MFC MODULE NOT INSTALLED!\n");
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
    static u8_t l1_alarm, l2_alarm;
    u8_t *led_red = (u8_t *)E1_RED_LED_ADDR;
    u8_t *led_green = (u8_t *)E1_GREEN_LED_ADDR;

    *led_green = (~(ram_params.e1_l1_alarm) & e1_params.e1_enable[card_id & 0xF]) & e1_params.e1_l2_alarm_enable[card_id & 0xF];
    *led_red = (ram_params.e1_l1_alarm | ~(ram_params.e1_l2_alarm)) & e1_params.e1_enable[card_id & 0xF];

    if (l1_alarm != ram_params.e1_l1_alarm || l2_alarm != ram_params.e1_l2_alarm) {
        l2_alarm = ram_params.e1_l2_alarm;
        l1_alarm = ram_params.e1_l1_alarm;
        
        //LOG_W("update led: l1_alarm = %x, back_value = %x", l1_alarm, ~l1_alarm);
        //LOG_W("update led: l2_alarm = %x, back_value = %x", l2_alarm, ~l2_alarm);        
    }
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
    //struct zl50020_cml *cml = ZL_CML;
    u8_t o_stream ;
    u8_t idle_code, test_e1, test_slot;

    if (!zl50020_inited) {
        zl50020_init();
    }

    u16_t data = dev->sicr[0];
    if (dev->sicr[0] != 0x1) {
        goto fault;
    }

    if (dev->sicr[1] != 0x1) {
        goto fault;
    }

    if (dev->socr[0] != 0x1) {
        goto fault;
    }

    if (dev->socr[1] != 0x1) {
        goto fault;
    }

#ifdef ZL50020_16M_CONNECTION
    if (dev->sicr[16] != 0x4 || dev->socr[16] != 0x4) {
        goto fault;
    }
#else
    if (dev->sicr[16] != 0x3 || dev->socr[16] != 0x3) {
        goto fault;
    }
#endif
    dev->ocfcr = 0x1cf;

    if (dev->ocfcr != 0x1cf) {
        goto fault;
    }

    idle_code = 0x57;
    test_e1 = 0;
    test_slot = 1;
    LOG_I("Now test ZL50020 and DS26518 slot connection, test_slot = %d, test_e1=%d, idle_code=%x",
        test_slot, test_e1, idle_code);
    ds26518_set_idle_code(test_e1,test_slot, idle_code, 1);

    set_ds26518_loopback(0, FRAME_LOCAL_LP);
    LOG_D("After Loopback on e1, e1 tx ==> rx, so, read zl50020 data memory , should be=%x", idle_code);
#ifdef ZL50020_16M_CONNECTION
    o_stream = (card_id & 0xf) + PCM_BUS_START_STREAM;
#else
    o_stream = (card_id & 0xf)*2 + PCM_BUS_START_STREAM;
#endif
    LOG_I("First test local stream, stream no = %d, slot=%d", test_e1, test_slot);
    HAL_Delay(2);
    read_zl50020_data_mem(test_e1, test_slot, idle_code);
    LOG_I("Then test PCM bus, stream no = %d, slot = %d", o_stream, test_slot);
    HAL_Delay(2);
    read_zl50020_data_mem(o_stream, test_slot, idle_code);
    
    /*Close test, and reset idle code and loopback */
    ds26518_set_idle_code(test_e1,test_slot, 0x37,0);
    set_ds26518_loopback(0, NO_LP);
    
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
    HAL_Delay(800);
    LED1_RED_ON;
    LED2_RED_ON;
    HAL_Delay(800);
    LED1_ORG_ON;
    LED2_ORG_ON;
    HAL_Delay(800);
    LED1_OFF;
    LED2_OFF;
	
	HAL_Delay(800);
	*led_green = 0xff;
	*led_red = 0;
    HAL_Delay(800);
    *led_green = 0x0;
	*led_red = 0xff;
    HAL_Delay(800);
    *led_green = 0xff;
	*led_red = 0xff;
	HAL_Delay(800);
    *led_green = 0;
	*led_red = 0;
}

void module_test(void)
{ 
    conf_module_detect();

    mfc_module_detect();
}
