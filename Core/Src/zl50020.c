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

#define CONF_C_ADDR     (CONF_ADDR + 1)

#define ZL_DEV (struct zl50020_dev *)ZL50020_BASE
#define ZL_CML (struct zl50020_cml *)ZL50020_CML_ADDR

//#define IBO_ENABLE

extern u8_t card_id;

static int zl50020_inited = 0;

void zl50020_connect_memory_init(u8_t stream_no)
{
    #if 0
    for(int i = 0; i < 31; i++) {
        u16_t *cml = (u16_t *)CML_STO_ADDR(i);
        for(int j = 0; j < 256; j++) {
            *(cml + j) = (2 << 9) | (0 << 1);
        }
    }
    #endif
    u16_t *cml = (u16_t *)CML_STO_ADDR(stream_no);
    for (int i = 0; i < 32; i++) {
        *(cml + i) = (TONE_STREAM << 9);
    }
}

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
    //dev->ims = 0;
    dev->ims = 1;
    HAL_Delay(5);
    LOG_W("dev->ims=%x", dev->ims);
    while (dev->ims != 0);
	
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

 
    LOG_D("ZL500020 REGISTER:");
    LOG_D("CR = %X, IMS=%x, SRR=%X, OCFCR=%X",dev->cr, dev->ims, dev->srr, dev->ocfcr);
    LOG_D("SICR1 = %X, SOCR1=%X, SICR8=%X, SOCR8=%X",
        dev->sicr[0], dev->socr[0], dev->sicr[8], dev->socr[8]);
    
    for (int i = 0; i < 11; i++){
        zl50020_connect_memory_init(i);
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

    if ((i_e1 >> 3) == 0) {
        if (i_e1 == TONE_E1) {
            i_stream = TONE_STREAM;
            i_slot = i_ts;
        } else {
            i_stream = i_e1 & 7;
            i_slot = i_ts;
        }       
        //LOG_I("local card switch, i_e1 = %d, card_id=%d, i_stream=%d, i_slot=0x%x", i_e1, card_id, i_stream, i_slot);
        
    } else {
        i_stream = (i_e1 >> 3) + 11;
        i_slot = i_ts;
    }

    if (o_e1 == TONE_E1) {
        o_stream = MFC_STREAM;
        o_slot = o_ts;
    } else {
        o_stream = o_e1;
        o_slot = o_ts;
    }

    //LOG_D("o_ts --- o_e1--- i_ts--- i_e1 === o_st === i_st");
    //LOG_D("0x%x\t0x%x\t0x%x\t0x%x\t%d\t%d", o_ts, o_e1, i_ts, i_e1,o_stream, i_stream);
    
    conn_value = (i_stream << 9) | (i_slot << 1);
  
    dev->cr &= 0xFC; // CONNECT MEMORY LOW read/write.

    cml->sto_connect[o_stream][o_slot] = conn_value;

    LOG_I("Connect slot 0x%x[%d] <-- 0x%x[%d]\t[%p]=0x%x", o_slot, o_stream, i_slot, i_stream,
        &cml->sto_connect[o_stream][o_slot], cml->sto_connect[o_stream][o_slot]);

}

/* i_stream possible value:
TONE_STREAM,
CONF_STREAM,
MFC_STREAM
*/
void connect_tone(uint16_t o_ts, uint16_t o_e1, uint16_t i_ts, uint16_t i_stream)
{
    struct zl50020_dev *dev = ZL_DEV;
    struct zl50020_cml *cml = ZL_CML;
    
    dev->cr &= 0xFC; 

    //pcml[o_slot] = (i_stream << 9) | (i_ts << 1); /* BIT 8 - BIT 1 */
    cml->sto_connect[o_e1][o_ts] = (i_stream << 9) | (i_ts << 1);

    LOG_I("connect tone 0x%x[%d] <-- 0x%x[%d]",
        o_ts, o_e1, i_ts, i_stream);
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
            if (dummy != 0x86) {
                LOG_E("Error!");
            }
        }
    }
    dev->cr = 0x84;
    LOG_HEX("", 16, data, 200);
}

void read_zl50020_data_mem(u8_t stream_no, u8_t slot)
{
    struct zl50020_cml *cml = ZL_CML;
    struct zl50020_dev *dev = ZL_DEV;
    u8_t data[300] = {0};
    u8_t dummy;

    LOG_I("Now print '%d' stream '%d' slot data memory!", stream_no, slot);
    dev->cr = 0x86; // data memory read.
    for(int i = 0; i < 300; i++) {
        data[i] = cml->sto_connect[stream_no][slot];
        for (int j = 0; j < 20*16; j++) {
            dummy = dev->cr;
            if (dummy != 0x86) {
                LOG_E("Error!");
            }
        }
    }
    dev->cr = 0x84;
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

void mfc_t32_zl50020_test(u8_t test_value)
{
    /* t32负责放A1-A7， mfc负责解码，zl50020负责连接时隙 */
    if (zl50020_inited == 0) {
        zl50020_init();
    }

    if (ram_params.mfc_module_installed == 0) {
        LOG_W("Not installed MFC Module, can't run the test!");
        return;
    }

    LOG_I("Now MFC-TONE32-ZL50020 test is going on!");

    for (int i = 0; i < 1; i++)
    {
        connect_slot(i, TONE_E1, test_value, TONE_E1);
        HAL_Delay(100);
        u8_t read_value = read_dtmf(i);
        LOG_W("--write\tread");
        LOG_W("--%x\t%x\t%x\t%x", test_value, read_value, read_dtmf(i), read_dtmf(i));
    }
    //LOG_I("TEST PASS!");
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
        /*
        LOG_W("update led: l1_alarm = %x", l1_alarm);
        LOG_W("update led: l2_alarm = %x", l2_alarm);
        LOG_W("update led: e1_enable = %x, l2_alarm_enable=%x", 
            e1_params.e1_enable[card_id & 0xF], e1_params.e1_l2_alarm_enable[card_id & 0xF]);
        */
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

    zl50020_init();

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
