/*
 * ds26518.c
 *
 *  Created on: 2020年6月11日
 *      Author: hly66
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ds26518.h"
#include "main.h"
#include "usart.h"
#include "zl50020.h"
#include "mtp.h"
#include "eeprom.h"
#include "lwip/sys.h"
#include "lwip/def.h"

#define LOG_TAG              "ds26518"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

/* FSMC_NE2 for ds26518 */
#define DS26518_BASE			(0x64000000)
#define DS26518_DEVICE			(DEVICE *)DS26518_BASE

//#define IBO_ENABLE

static FRAMER *ds26518_global_framer(void)
{
	DEVICE *dev = DS26518_DEVICE;
	return  (FRAMER *)&dev->te1[0];
}

static void delay_us(void)
{
	FRAMER *f = ds26518_global_framer();
	UC i = f->idr;

	for(i=0; i < 10; i++){
		;
	}
}

static FRAMER *ds26518_framer(int e1_no)
{
	DEVICE *dev = DS26518_DEVICE;
	return  (FRAMER *)&dev->te1[e1_no];
}

static LIU *ds26518_liu(int e1_no)
{
	DEVICE *dev = DS26518_DEVICE;
	return (LIU *)&dev->liu[e1_no];
}

static BERT *ds26518_bert(int e1_no)
{
	DEVICE *dev = DS26518_DEVICE;
	return (BERT *)&dev->bert[e1_no];
}

static EXTBERT *ds26518_exbert(int e1_no)
{
    DEVICE *dev = DS26518_DEVICE;
	return (EXTBERT *)&dev->extbert[e1_no];
}

#if 0
static HDLC *ds26518_hdlc(int e1_no)
{
	DEVICE *dev = DS26518_DEVICE;
	return (HDLC *)&dev->hdlc[e1_no];
}
#endif

void set_ds26518_interrupt(int e1_no, int enable)
{
	FRAMER *f = ds26518_global_framer();

	if (enable) {
		f->gfimr1 |= (1 << e1_no);
		f->glimr1 |= (1 << e1_no);
		//LOG_I("Enable '%d' E1 interrupt", e1_no);
	} else {
		f->gfimr1 &= ~(1 << e1_no);
		f->glimr1 &= ~(1 << e1_no);
		//LOG_I("Disable '%d' E1 interrupt", e1_no);
	}
}

void set_ds26518_global_interrupt(int enable)
{
	FRAMER *f = ds26518_global_framer();
	if (enable) {
		f->gtcr1 &= ~GTCR1_GIPI;
		//LOG_I("Enable E1 global interrupt");
	} else {
		f->gtcr1 |= GTCR1_GIPI;
		//LOG_I("Disable E1 global interrupt");
	}
}

void ds26518_set_idle_code(u8_t e1_no, u8_t slot, u8_t idle_code, u8_t flag)
{
    FRAMER *f = ds26518_framer(e1_no);
    
    if (flag) {
        f->tidr[slot] = idle_code;
        ds26518_e1_slot_enable(e1_no, slot, VOICE_INACTIVE);
    } else {
        f->tidr[slot] = 0xD5;
        ds26518_e1_slot_enable(e1_no, slot, VOICE_ACTIVE);
    }
}

static void ds26518_tcice_init(int e1_no)
{
	FRAMER *f = ds26518_framer(e1_no);

	f->tcice[0] = 0xfe; /* slot 0 disable send idle code*/
	f->tcice[1] = 0xff;
	f->tcice[2] = 0xfe; /* slot 16 disable */
	f->tcice[3] = 0xff;
}

static void ds25618_ts_init(int e1_no)
{
	FRAMER *f = ds26518_framer(e1_no);

	f->ts[0] = 0x0b; // 时隙0的高四位必须为0， 用于复帧同步信号(CAS MF)

	memset((void *)&f->ts[1], 0xBB, 15); // 0b 表示线路空闲。

	int base = e1_no << 5;

	slot_params[base].ls_out = 0;
	slot_params[base].ls_in = 0;

	for (int i = 1; i < MAX_E1_TIMESLOTS; i++) {
		slot_params[base + i].ls_out = 0xb;
		slot_params[base + i].ls_in = 0;
	}
}

void read_rx_abcd(int e1_no, u8_t *rv_abcd)
{
	FRAMER *f = ds26518_framer(e1_no);
	u8_t value;
    //u8_t flag = 0;
	
	for (int i = 1; i < MAX_E1_TIMESLOTS / 2; i++) {
		value = f->rs[i];
		*(rv_abcd + i) = ((value & 0xf0) >> 4);
		*(rv_abcd + i + 16) = (value & 0x0f);
	}
    
//    if (flag) {
//        LOG_HEX("rx-abcd", 16, rv_abcd, 32);
//    }
}

void out_tx_abcd(int e1_no, u8_t slot, u8_t value)
{
	FRAMER *f = ds26518_framer(e1_no);

	//u8_t old_value = f->ts[slot & 0x10];

	if (slot == 0 || slot == 0x10) {
		return;
	}
	u8_t tot_index = (e1_no << 5) + slot;
	slot_params[tot_index].ls_out = value & 0xf;

	u8_t index = (slot > 15) ? (slot - 16) : slot;
	u8_t bits = value & 0xf;
	if (slot > 15) {
		bits = (slot_params[tot_index - 16].ls_out << 4) | bits;
	} else {
		bits = (slot_params[tot_index + 16].ls_out) | (bits << 4);
	}

	f->ts[index] = bits;

	LOG_I("'%d' E1 '%d' slot SET line code is %02x, ts value=%02x", 
        e1_no, slot, value, f->ts[index]);
}

u8_t read_liu_status(int e1_no)
{
	static u8_t liu_status[8];

	LIU *l = ds26518_liu(e1_no);

	u8_t ret = (l->lrsr > 0 ? 1 : 0);
	//l->llsr = ret;

	if (liu_status[e1_no & 7] != ret) {
		LOG_W("link '%d' liu status = %x", e1_no, ret);
		liu_status[e1_no & 7] = ret;
	}

	return ret;
}

char *get_lrsr_value(u8_t lrsl)
{
	u8_t value = lrsl >> 4;
	static char *signal_level[] = {
		">-2.5",
		"-2.5 to -5",
		"-5 to -7.5",
		"-7.5 to -10",
		"-10 to -12.5",
		"-12.5 to -15",
		"-15 to -17.5",
		"-17.5 to -20",
		"-20 to -22.5",
		"-22.5 to -25",
		"-25 to -27.5",
		"-27.5 to -30",
		"-30 to -32.5",
		"-32.5 to -35",
		"-35 to -37.5",
		"<-37.5"
	};

	return signal_level[value & 0xf];
}
/* >0 mean liu has some problem */
u8_t check_liu_status(int e1_no)
{
	LIU *l = ds26518_liu(e1_no);
	u8_t llsr = l->llsr;
	u8_t lrsl = l->lrsl;
	u8_t lrsr = l->lrsr;

	if (llsr & 1){
		if (lrsr & 1) {
			LOG_W("Loss of Signal Detect (LOSD) on %d LINK\r\n", e1_no);
		}else {
			LOG_W("Loss of Signal Detect (LOSD) on %d LINK, but real status not show", e1_no);
		}
	}
	if (llsr & 2){
		
		LOG_W("Short-Circuit Detect (SCD) on %d LINK, lrsr=%x", e1_no, lrsr);
	}
	if (llsr & 4){
		LOG_W("Open-Circuit Detect (OCD) on %d LINK, lrsr=%x", e1_no, lrsr);
	}
	if (llsr & 0x10){
		LOG_W("Loss of Signal Clear (LOSC) on %d LINK\r\n", e1_no);
	}
	if (llsr & 0x20){
		LOG_W("Short-Circuit Clear (SCC) on %d LINK\r\n", e1_no);
	}
	if (llsr & 0x40){
		LOG_W("Open-Circuit Clear (OCC) on %d LINK\r\n", e1_no);
	}
	l->llsr = llsr;
	LOG_I("LIU Receive signal Level = %s on %d LINK\r\n", get_lrsr_value(lrsl), e1_no);
	return llsr;
}

void read_e1_status(void)
{
    static u8_t l1_status = 0;
	LIU *l;
	u8_t status, llsr;
	u8_t siglost = 0;
    
	for(int i = 0; i < E1_PORT_PER_CARD; i++) {
		l = ds26518_liu(i);
        llsr = l->llsr;
		if ((llsr & 0xf) > 0 || (l->lrsl >> 4) > 6) {
			/* Bit 3: Jitter Attenuator Limit Trip Set
			* Bit 2: Open-Circuit Detect
			* Bit 1: Short-Circuit Detect
			* Bit 0: Loss of Signal Detect
			*/
			status = 1;
		} else {
			status = 0;
		}

		siglost |= (status << i);
		l->llsr = llsr;
	}
    
    if (l1_status != siglost) {
        LOG_W("L1 status changed! %x --> %x", l1_status, siglost);
        l1_status = siglost;
    }
    
	ram_params.e1_l1_alarm = siglost & (~(ram_params.conf_module_installed << 6));
	
	for(int i = 0; i < E1_PORT_PER_CARD; i++) {
		u8_t j = (siglost >> i) & 1;
		if (ram_params.e1_status[i] != j) {
			if (ram_params.e1_status_last[i] > 5) {
				LOG_W("E1 '%d' change L1 status %x <-- %x", i, j, ram_params.e1_status[i]);
				ram_params.e1_status[i] = j;
				if (j == 0 && is_ccs_port(i)) {
					/*Link is connected!*/
					start_mtp2_process(i);
				}
			} else {
				ram_params.e1_status_last[i]++;
			}
		} else {
			ram_params.e1_status_last[i] = 0;
		}
	}
}

void ds26518_e1_slot_enable(int e1_no, int slot, enum SLOT_ACTIVE active)
{
	FRAMER *f = ds26518_framer(e1_no);

	if (slot == 0 || slot == 16) {
		return;
	}
	
	u8_t index = (u8_t)(slot >> 3);
	u8_t bit_mask = (1 << (u8_t)(slot & 0x7));

	if (active == VOICE_ACTIVE) {
		f->tcice[index] &= (~bit_mask);
	} else {
		f->tcice[index] |= bit_mask;
	}
/**
	LOG_D("'%d' E1 '%d' slot set %s , tcice=%02x %02x %02x %02x",
        e1_no, slot, active == VOICE_ACTIVE ? "Enable":"Disable", 
        f->tcice[0], f->tcice[1], f->tcice[2], f->tcice[3]);
*/
}


void set_ds26518_backplane_refclock(enum BACKPLANE_REFERENCE back_ref)
{
	FRAMER *f = ds26518_global_framer();

	f->gtccr1 = (f->gtccr1 & 0x0f) | (back_ref << 4);

	delay_us();
}

/*
 * 设置每个E1端口的2.048M发送时钟是从TCLKn获取还是REFCLKIO信号获取，
 * 当前芯片如果莫格端口作为系统参考时钟时，则设为TCLK_PIN，如果其从其他DS26518芯片获取主时钟
 * 则设为TCLK_REFCLKIO.
 * 系统默认的初始化是设为TCLK_PIN.
 */
void set_ds26518_tclk_src(enum TCLK_REFERNCE tclk_ref)
{
	FRAMER *f = ds26518_global_framer();

	/* BIT4 for TCLK */
	f->gtccr3 = (f->gtccr3 & 0xEF) | (tclk_ref << 4);
}

void set_ds26518_master_clock(enum BACKPLANE_REFERENCE back_ref)
{
	FRAMER *f = ds26518_global_framer();

	if (back_ref > MCLK) {
		back_ref = MCLK;
	}
	set_ds26518_backplane_refclock(back_ref);
	//set_ds26518_tclk_src(TCLK_PIN);

}

void set_ds26518_slave_clock(void)
{
	FRAMER *f = ds26518_global_framer();

	set_ds26518_backplane_refclock(REFCLKIO); 
	//set_ds26518_tclk_src(TCLK_PIN);

	//f->gtcr3 = 0x1;
	//f->gtccr3 = 0;
}

void set_ds26518_signal_slot(int e1_no, UC signal_slot)
{
	if (signal_slot < 1 || signal_slot > 31)
		return;

	FRAMER *f = ds26518_framer(e1_no);

	UC old_signal_slot = f->thc2 & 0x1F;
	if (signal_slot != old_signal_slot){
		f->thc2 = (f->thc2 & 0xE0) | signal_slot;
		f->rhc = (f->rhc & 0xE0) | signal_slot;

		f->thc1 |= THC1_THR; //Reset transmit HDLC-64 controller and flush the transmit FIFO.
		f->rhc  |= RHC_RHR; // Reset receive HDLC-64 controller and flush the receive FIFO.

		while(f->thc1 & THC1_THR);
		while(f->rhc & RHC_RHR);
	}
}

UC read_ds26518_signal_slot(int e1_no)
{
	FRAMER *f = ds26518_framer(e1_no);

	return (f->thc2 & 0x1F);
}

void set_ds26518_crc4(int e1_no, int enable)
{
	FRAMER *f = ds26518_framer(e1_no);
	if (enable) {
		f->tcr1 |= (TCR1_TCRC4);
        f->rcr1 |= (RCR1_RCRC4);
	} else {
		f->tcr1 &= ~(TCR1_TCRC4);
        f->rcr1 &= ~(RCR1_RCRC4);
	}
}

void ds26518_global_init(void)
{
	uint32_t i;
	UC *ucp;
	UC dummy;

	FRAMER *f = ds26518_global_framer();
	ucp = (UC *)f;

	//STEP1: reset register.
	f->gsrr1 = GFSRR1_H256RST| GFSRR1_LRST| GFSRR1_BRST| GFSRR1_FRST;

	f->gsrr1 = 0;

	/* GTCCR1:
	 * BIT7-BIT4: BPREFSEL3-BPREFSEL0 背板参考时钟选择
	 * 						0-RCLK1
	 * 						1-RCLK2
	 * 						2-RCLK3
	 * 						3-RCLK4
	 * 						4-RCLK5
	 * 						5-RCLK6
	 * 						6-RCLK7
	 * 						7-RCLK8
	 * 						8-MCLK, REFCLKIO is an output
	 * 						9-REFCLKIO, REFCLKIO is an input
	 * BIT3: 	  BFREQSEL  0-Backplane reference clock is 2.048MHz.
	 * 			  			1-Backplane reference clock is 1.544MHz.
	 * BIT2:      FREQSEL   0-The external master clock is 2.048MHz or multiple thereof.
	 * 						1-The external master clock is 1.544MHz or multiple thereof.
	 * BIT1-BIT0: Master Period Select 1 and 0 (MPS[1:0])
	 * 			  MPS = 00, MCLK = 2.048M
	 * 			  MPS = 01, MCLK = 4.096M
	 * 			  MPS = 10, MCLK = 8.192M
	 * 			  MPS = 11, MCLK = 16.384M
	 */
	if ((card_id & 0xf) > 0) {
		/* REFCLKIO is input */
		f->gtccr1 = 0x90;
	}else {
		/* 2.048MHz derived from MCLK. (REFCLKIO is an output.) */
		f->gtccr1 = 0x80; 
	}
	
	LOG_W("REF CLOCK = %x", f->gtccr1);
	
	dummy = f->idr;		/* delay at least 300 ns */
	dummy = f->idr;

	LOG_D("DS26518 ID=%02X", dummy);

	for(i = 0; i < sizeof(DEVICE); i++, ucp++) {
		if(i >= 0x00f0 && i <= 0x00ff)
			continue;
		if(i >= 0x1180 && i <  0x1400)
			continue;
		if(i >= 0x1480 && i <  0x1500)
			continue;
		if(i >= 0x1600 && i <  sizeof(DEVICE))
			continue;
		*ucp = 0;
	}
    HAL_Delay(1);
	/* GTCR1
	 * RLOF output,
	 * 528MD disable,
	 * disable global interrupt
	 * use internal ibo mux
	 * */
	f->gtcr1 = GTCR1_GIPI ; 

	/* GFCR1
	 * BIT7-BIT6 = 3: IBO MODE: 16.384M (8 devices on TSER1 and RSER1)
	 * BIT5-BIT4 = 3: BPCLK1: 	 16.384M CLK OUTPUT.
	 * RMSYNC/RFSYNC[8:1] pins output RFSYNC[8:1](Receive Frame Sync)
	 * TCHBLK/TCHCLK[8:1] pins output TCHBLK[8:1] (Transmit Channel Block)
	 * RCHBLK/RCHCLK[8:1] pins output RCHBLK[8:1] (Receive Channel Block)
	 */
	/* GTCR3
	 * Bit 1 = 0, TSSYNCIOn are Input PIN.
	 * 		 = 1, TSSYNCIOn are Output sync to BPCLK1.
	 * Bit 0 = 0, TSYNCn is selected for TSYNC/TSSYNCIO[8:1] pins
	 *       = 1, TSSYNCIOn is selected for TSYNC/TSSYNCIO[8:1] pins.
	 */

	f->gfcr1 = GFCR1_IBOMS(0) | GFCR1_BPCLK(3); // disable IBO, 8.192M BPCLK1
	f->gtcr3 = 0x0;


	/*GTCCR3
	 * BIT6 = 1 Use BPCLK1 as the master clock for all eight receive system clocks (Channels 1–8).
	 * 		= 0 Use RSYSCLKn pins for each receive system clock (Channels 1–8).
	 * BIT5 = 1 Use BPCLK1 as the master clock for all eight transmit system clocks (Channels 1–8).
	 * 		= 0 Use TSYSCLKn pins for each transmit system clock (Channels 1–8).
	 * BIT4 = 0: Use TCLKn pins for each of the transmit clock (Channels 1–8).
	 * 		= 1: Use REFCLKIO as the master clock for all eight transmit clocks (Channels 1–8).
	 */

	f->gtccr3 = 0;

}

static void ds26518_hdlc64_init(uint8_t e1_no)
{
	FRAMER *f = ds26518_framer(e1_no);

	f->rhbse = 0; // Receive HDLC-64 Bit Suppress Register 不使用位压缩
	f->thbse = 0; // Transmit HDLC-64 Bit Suppress Register 不使用位压缩

	f->rhfc = 0x2; //RHFC Receive HDLC-64 FIFO Control Register, 设置接收FIFO的高水位字节=32bytes
	f->thfc = 0x2; // //THFC Transmit HDLC-64 FIFO Control Register, 设置发送FIFO的高水位字节=32bytes

	/*
	 * THC2 Transmit HDLC-64 Control Register 2
	 * Bit 7: Transmit Abort(TABT). 置1表示清除发送FIFO中的内容，发送放弃字节0xFE和Flag0x7E。
	 * Bit 6: Send BOC (SBOC) (T1 Mode Only).
	 * Bit 5: Transmit HDLC-64 Controller Enable (THCEN) 置1打开HDLC-64开关
	 * Bits 4 to 0: Transmit HDLC-64 Channel Select (THCS[4:0]).
	 * Changes to this value are acknowledged only upon a transmit HDLC-64 controller reset (THR at THC1.5).
	 */
	f->thc2 = THC2_THCEN | THC2_THCS(16);  //使能发送HDLC-64， 信令channle=16 此处应该设为16

	/*
	 * RHC: Receive HDLC-64 Control Register
	 * Bit7: 0 - Do not write received CRC-16 code to FIFO (default)
	 * 		 1 - Write received CRC-16 code to FIFO after last octet of packet
	 * Bit6: Receive HDLC-64 Reset (RHR)
	 * 		 0 - Normal operation
	 * 		 1 - Reset receive HDLC-64 controller and flush the receive FIFO
	 * Bit5: Receive HDLC-64 Mapping Select (RHMS)
	 * 		 0 - Receive HDLC-64 assigned to channels.
	 * 		 1 - Receive HDLC-64 assigned to FDL (T1 mode), Sa bits (E1 mode).
	 * Bit4-Bit0: Receive HDLC-64 Channel Select 4 to 0 (RHCS[4:0])
	 * 			只有RHR置1复位后，才能修改此值
	 */
	f->rhc = RHC_RCRCD | RHC_RHCS(16); // signal slot = 16, Write received CRC-16 code to FIFO after last octet of packet.
	f->rhc  |= RHC_RHR; // Reset receive HDLC-64 controller and flush the receive FIFO.

	/*
	 * THC1 Transmit HDLC-64 Control Register 1
	 * Bit 7: Number of Flags Select(NOFS)  0 - 在连续消息间插入一个flag（0x7E）， 1 - 插入2个Flag
	 * Bit 6: Transmit End of Message and Loop (TEOML). 1 - 循环发送最后一个消息直到新消息写入或置0
	 * Bit 5: Transmit HDLC-64 Reset (THR). 1 - 复位THDLC-64
	 * Bit 4: Transmit HDLC-64 Mapping Select (THMS) 0 - Transmit HDLC-64 assigned to channels
	 * Bit 3: Transmit Flag/Idle Select (TFS). 0 - FLAG=0x7E， 1 - Flag = 0xFF
	 * Bit 2: Transmit End of Message (TEOM). 在发送消息最后一个字节前需要将此位置1
	 * Bit 1: Transmit Zero Stuffer Defeat (TZSD). 通常置0
	 * Bit 0: Transmit CRC Defeat (TCRCD). 0 - 允许CRC， 1 - 禁止CRC
	 */
	f->thc1 = 0; // 允许CRC
	f->thc1 |= THC1_THR; //Reset transmit HDLC-64 controller and flush the transmit FIFO.

	/*
	 * TIM2 Transmit Interrupt Mask Register 2 (HDLC-64)
	 * Bit 3: Transmit FIFO Underrun Event (TUDR)
	 * Bit 2: Transmit Message End Event (TMEND)
	 * Bit 1: Transmit FIFO Below Low Watermark Set Condition (TLWMS)
	 * Bit 0: Transmit FIFO Not Full Set Condition (TNFS)
	 */
	f->tim2 = TIM2_TMEND | TIM2_TLWMS ;

	/*
	 * RIM5 Receive Interrupt Mask 5 (HDLC-64)
	 * Bit 5: Receive FIFO Overrun
	 * Bit 4: Receive HDLC-64 Opening Byte Event (RHOBT)
	 * Bit 3: Receive Packet End Event (RPE)
	 * Bit 2: Receive Packet Start Event (RPS)
	 * Bit 1: Receive FIFO Above High Watermark Set Event (RHWMS)
	 * Bit 0: Receive FIFO Not Empty Set Event (RNES)
	 */
	f->rim5 = RIM5_RPE | RIM5_RHWMS;
}

void ds26518_port_init(int e1_no, enum SIG_TYPE sig_type)
{
	
	LIU *l = ds26518_liu(e1_no);
	FRAMER *f = ds26518_framer(e1_no);

	//先关闭全局FRAMER中断
	set_ds26518_interrupt(e1_no, 0);

	//设置发送端参数
	f->tmmr = TMMR_FRM_EN | TMMR_T1E1;

	/* TCR1
	 * Bit 7 = 0, FAS bits/Sa bits/Remote Alarm sourced internally from the E1TAF and E1TNAF registers.
	 * Bit 6 = 0, Time slot 16 determined by the SSIE1–4 and THSCS1–4 registers. for CCS MODE.
	 * 	     = 1, Source time slot 16 from TS1–16 registers. for CAS MODE.
	 * Bit 5 = 0, Do not force TCHBLKn high during bit 1 of time slot 26.
	 * Bit 4 = 1, Source Si bits from E1TAF and E1TNAF registers.
	 * Bit 3 = 0, Normal operation.
	 *       = 1, Force time slot 16 in every frame to all ones.
	 * Bit 2 = 1, HDB3 enabled.
	 * Bit 1 = 0, Transmit data normally.
	 *       = 1, Transmit an unframed all-ones code at TTIPn and TRINGn.
	 * Bit 0 = 0, CRC-4 disabled.
	 *       = 1, CRC-4 enabled.
	 */
	f->tcr1 = TCR1_TSIS | TCR1_THDB3;
	//f->tcr1 = TCR1_TSIS ;
	if (sig_type == CAS_TYPE) {
		f->tcr1 |= TCR1_T16S | TCR1_THDB3;
		ds25618_ts_init(e1_no);
	} 

	/* TIBOC
	 * Bit 4 = 1, Frame Interleave.
	 * Bit 3 = 1, Interleave Bus Operation enabled.
	 */
#ifdef IBO_ENABLE
	f->tiboc = TIBOC_IBOSEL | TIBOC_IBOEN;
#else
	f->tiboc = 0;
#endif

	/* E1AF, MUST be 0x1b */
	f->tslc1_taf = 0x9b;

	/* E1NAF, MUST be 0x40 */
	f->tslc2_tnaf = 0xdf;

	/* TIOCR
	 * Bit7 = 0, TCLKn No inversion.
	 * Bit6 = 0, TSYNCn No inversion.
	 * Bit5 = 0, TSSYNCIOn No inversion.
	 * 
	 * Bit4 = 1, If TSYSCLKn is 2.048/4.096/8.192MHz or IBO enabled.
	 * Bit3 = 0, TSSYNCIOn Mode Select Frame mode.
	 * Bit2 = 0, TSYNCn I/O Select input,  1: TSYNC I/O Select output.
	 * Bit0 = 0, TSYNCn Mode Select Frame mode.
	 */
	/* Disable Transmit Elastic store */
	f->tescr = 0;
	f->tiocr = TIOCR_TSCLKM;  //TSYNC为输入信号
	//f->tiocr = TIOCR_TSIO | TIOCR_TSCLKM; //定义TSYNC为输出
	
	/* TXPC
	 * Bit 7 = 0 ,Transmit HDLC-256 Mode Select assigned to time slots
	 * Bit 6 = 0, Transmit HDLC-256 is not active.
	 * Bit 2 = 0, Normal (line) operation. Transmit BERT port sources data into the transmit path.
	 * Bit 0 = 0, Transmit BERT port is not active.
	 */
	f->txpc = 0;

	/* hdlc256 slot disable */
	memset((void *)f->thcs, 0, 4);

	/* Software-Signaling Insertion Enable Registers 1 to 4 */
    #if 0
	if (sig_type == CAS_TYPE) {
		f->ssie[0] = 0xFE;
		f->ssie[1] = 0xFF;
		f->ssie[2] = 0xFE;
		f->ssie[3] = 0xFF;
	} else {
		memset((void *)f->ssie, 0, 4);
	}
    #endif
    
	/* The Transmit Idle Code Definition Registers */
	memset((void *)f->tidr, 0xD5, 32);

	/* The Transmit Channel Idle Code Enable registers */
	ds26518_tcice_init(e1_no);

	f->tsacr = 0x0;

	/* 设置接收端参数 */
	//f->rmmr |= RMMR_FRM_EN | RMMR_T1E1 | RMMR_DRSS;
	f->rmmr |= RMMR_FRM_EN | RMMR_T1E1 ;

	/* RCR1
	 * Bit 6 = 1, HDB3 enabled
	 * Bit 5 = 0, CAS mode.
	 *       = 1, CCS mode.
	 * Bit 4 = 0, Receive G.802 disable, Do not force RCHBLKn high during bit 1 of time slot 26.
	 * Bit 3 = 0, CRC-4 disabled.
	 * Bit 2 = 1, Resync if FAS or bit 2 of non-FAS is received in error three consecutive times.
	 * Bit 1 = 0, Auto resync enabled.
	 */
	if (sig_type == CAS_TYPE) {
		f->rcr1 = RCR1_RHDB3 | RCR1_FRC;
	} else {
		f->rcr1 = RCR1_RSIGM | RCR1_RHDB3 | RCR1_FRC;
	}

	/* RIOCR
	 * Bit 7 = 0, RCLKn No inversion
	 * Bit 6 = 0, RSYNCn No inversion
	 * Bit 5 = 0, H.100 Sync Mode in Normal operation.
	 * Bit 4 = 1, RSYSCLKn is 2.048MHz or IBO enabled.
	 * Bit 2 = 1, RSYNCn is an input (only valid if elastic store enabled).
	 * Bit 1 = 0, RSYNCn outputs CAS multiframe boundaries.
	 * Bit 0 = 0, RSYNC Mode Select is Frame mode.
	 */
	f->riocr = RIOCR_RSCLKM | RIOCR_RSIO ;

	/* RESCR
	 * Bit 0: Receive Elastic Store Enable (RESE)
	 */
	f->rescr = RESCR_RESE;

	/* RIBOC
	 * Bit 4 = 1, Frame Interleave.
	 * Bit 3 = 1, Interleave Bus Operation enabled.
	 */
#ifdef IBO_ENABLE
	f->riboc = RIBOC_IBOSEL | RIBOC_IBOEN;
#else
	f->riboc = 0;
#endif


	l->ltrcr = 0x0; //Jitter attenuator FIFO depth 128 bits and in the receive path. E1,G.775
	l->ltipsr = LTIPSR_TIMPTON; //Enable transmit terminating impedance.E1, 75Ohm

	l->lrismr = LRISMR_RIMPON | LRISMR_RIMPM(4); //75Ω internal termination

	l->lrcr = 3; //43db for E1

	/* after the configuration turn on the framer */
	f->tmmr |= TMMR_INIT_DONE;
	f->rmmr |= RMMR_INIT_DONE;

	/* Turn on LIU output */
	l->lmcr = LMCR_TE;

	/* set hdlc64 */
	if (sig_type == CCS_TYPE){
		ds26518_hdlc64_init(e1_no);
	} else {
        f->thc2 = 0;
        f->rhc = 0;
    }

	HAL_Delay(1);
	
	/* Open interrupt */
	f->rim1 |= RIM1_RLOSD | RIM1_RLOFD | RIM1_RLOSC | RIM1_RLOFC;
	f->rim2 = RIM2_RAF;
	//f->rim4 = RIM4_RESF | RIM4_RSLIP;

	f->tim1 = TIM1_TESF | TIM1_TSLIP | TIM1_LOTC;
	f->tim3 = TIM3_LOFD;

	set_ds26518_interrupt(e1_no, 1);

	/* Last interrupt global active step */
	set_ds26518_global_interrupt(1);
#if 0    
	ds26518_mon_test2(e1_no, 1);
	set_ds26518_loopback(e1_no, FRAME_LOCAL_LP);
	HAL_Delay(1);
	ds26518_mon_test2(e1_no, 1);
#endif
LOG_D("E1 port '%d' init finished ON %s TYPE!", e1_no, sig_type == CCS_TYPE ? "CCS_TYPE" : "CAS_TYPE");

}

void reset_hdlc64_transmit(int e1_no)
{
	FRAMER *f = ds26518_framer(e1_no);
	f->thc1 |= THC1_THR;
	while(f->thc1 & THC1_THR);
}

void reset_hdlc64_receive(int e1_no)
{
	FRAMER *f = ds26518_framer(e1_no);
	f->rhc |= RHC_RHR;
	while(f->rhc & RHC_RHR);
}

void disable_e1_transmit(int e1_no)
{
	LIU	*liu = ds26518_liu(e1_no);

	liu->lmcr = LMCR_TPDE | LMCR_RPDE ;
}

void enable_e1_transmit(int e1_no)
{
	LIU	*liu = ds26518_liu(e1_no);

	liu->lmcr = LMCR_TE;
}

void set_ds26518_loopback(int e1_no, enum LOOPBACK_TYPE lp_type)
{
	LIU *liu = ds26518_liu(e1_no);
	FRAMER *f = ds26518_framer(e1_no);

	switch (lp_type) {
	case NO_LP:
		liu->lmcr = (liu->lmcr & 0xC7);
		f->rcr3 &= 0xFC;
		break;
	case REMOTE_JA_LP:
	case ANALOG_LP:
	case REMOTE_NO_JA_LP:
	case LOCAL_LP:
	case DUAL_LP:
		liu->lmcr = (liu->lmcr & 0xC7) | (lp_type << 3);
		break;
	case FRAME_LOCAL_LP:
		f->rcr3 |= 0x1;
		break;
	case FRAME_REMOTE_LP:
		f->rcr3 |= 0x2;
	default:
		break;
	}
}

void ds26518_tx_set(u8_t e1_no, u8_t *buf, u8_t len, u8_t end_flag)
{
	u8_t i;
	FRAMER *f = ds26518_framer(e1_no);

	for (i = 0; i < len - 1; i++) {
		f->thf = buf[i];
	}

	if (end_flag == 1) {
		// Last Byte to Send.
        f->thc1 |= THC1_TEOM; // bit2 of thc1 TEOM
	}
	f->thf = buf[i];
}

void ds26518_send_sio_test(void)
{
	u8_t buf[4] = {0xff, 0xff, 1, 0};
	FRAMER *f = ds26518_framer(0);

	u8_t count = f->tfba;
	if (count > 4) {
		if(f->thc1 & THC1_TEOM){
			f->thc1 &= (~THC1_TEOM);
			while (f->thc1 & THC1_TEOM);
			LOG_I("CLEAR THC1_TEMO flag, thc1=%2X", f->thc1);
		}
		for (int i = 0; i < 3; i++) {
			f->thf = buf[i];
		}

		f->thc1 |= THC1_TEOM; // bit2 of thc1 TEOM
		f->thf = buf[3];
		LOG_I("Send SIO end!");
	}
}

#if 0
void ds26518_isr(void)
{
	u8_t index;
	FRAMER *f = ds26518_global_framer();

	u8_t frame_isr_st = f->gfisr1;
	u8_t liu_isr_st = f->glisr1;

	for (index = 0; index < E1_LINKS_MAX; index++) {
		if (frame_isr_st & (1 << index)) {
			f = ds26518_framer(index);
			u8_t tiir = f->tiir;			
			if ( tiir & 2) {
				u8_t tls2 = f->tls2;
				if (tls2 & TLS2_TMEND || tls2 & TLS2_TLWMS) {

					u8_t count = f->tfba;
					if (count > 0) {

						if(f->thc1 & THC1_TEOM){
							f->thc1 &= (~THC1_TEOM);
						}
						send_ccs_msg(index, count);
					}
				}
				f->tls2 = tls2;
			}

			u8_t riir = f->riir;
			if (riir & 0x10) {  //RLS5
				u8_t rls5 = f->rls5;
				if (rls5 & RLS5_RNES || rls5 & RLS5_RHWMS) {
					u8_t rv_status = f->rhpba;
					u8_t rv_len = rv_status & 0x7F;

					while (rv_len) {
						rv_ccs_byte(index, f->rhf);
						rv_len--;	
					}				
					//MS = 0, received last N bytes of message.
					if ((rv_status & RHPBA_MS) == 0) { 
						rv_status =  ((f->rrts5) >> 4) & 0x7;
						if (rv_status == 1) {
							/* Receive OK */
							check_ccs_msg(index);
						}  else if (rv_status == 0) {
							continue;
						} else {
							/* Error */
							bad_msg_rev(index, rv_status);
						}
					}
				}
			}
		}

		if (liu_isr_st & ( 1 << index)) {
			LIU *l = ds26518_liu(index);
			u8_t llsr = l->llsr;
/* 		Bit6: Open-Circuit Clear (OCC). This latched bit is set when an open circuit condition was detected at TTIPn and TRINGn and then removed
		Bit5: Short-Circuit Clear (SCC). This latched bit is set when a short circuit condition was detected at TTIPn and TRINGn and then removed
		Bit4: Loss of Signal Clear (LOSC). This latched bit is set when a loss of signal condition was detected at RTIPn and RRINGn and then removed.
		Bit 2: Open-Circuit Detect (OCD). This latched bit is set when open-circuit condition is detected at TTIPn and TRINGn
		Bit 1: Short-Circuit Detect (SCD). This latched bit is set when short-circuit condition is detected at TTIPn and TRINGn
		Bit 0: Loss of Signal Detect (LOSD). This latched bit is set when an LOS condition is detected at RTIPn and RRINGn  */ 
			

			l->llsr = llsr;
		}
	}
}
#endif

/* Below for Ds26518 poll function */

void ds26518_tx_rx_poll(int e1_no)
{
	FRAMER *f = ds26518_framer(e1_no & 7);

	u8_t rv_status = f->rhpba;
	u8_t rv_len = rv_status & 0x7f;

	if (rv_status != 0x80) {
		while (rv_len > 0) {
			rv_ccs_byte(e1_no, f->rhf);
			rv_len--;
		}

		if ((rv_status & RHPBA_MS) == 0) {
			rv_status = ((f->rrts5) >> 4) & 0x7;
			if (rv_status == 1) {
				check_ccs_msg(e1_no);
			} else if (rv_status > 0) {
				bad_msg_rev(e1_no, rv_status);
			} 
		}
	}
	
	u8_t count = f->tfba;
	if (count > 0) {
		if(f->thc1 & THC1_TEOM){
			f->thc1 &= (~THC1_TEOM);
			while(f->thc1 & THC1_TEOM);
		}
		send_ccs_msg(e1_no, count);
	}
}

void ds26518_test(void)
{
	
	FRAMER *f = ds26518_global_framer();

	ds26518_global_init();

	init_mtp2_mem();

	for(int i = 0; i < E1_PORT_PER_CARD; i++) {
		ds26518_port_init(i, CCS_TYPE);
	}

	if (f->gsrr1 != 0) {
		goto fault;
	}
#if 0
	if (f->gtcr1 != GTCR1_GIPI) {
        LOG_W("GTCR1 = 0x%x", f->gtcr1);
		goto fault;
	}
#endif
	if (f->gfcr1 != GFCR1_BPCLK(3)) {
        LOG_W("GFCR1 = 0x%x", f->gfcr1);
		goto fault;
	}

	if (f->gtcr3 != 0) {
        LOG_W("GTCR3 = 0x%x", f->gtcr3);
		goto fault;
	}

	if (f->gtccr3 != 0) {
        LOG_W("GTCCR3 = 0x%x", f->gtccr3);
		goto fault;
	}
    
    if ((card_id & 0xf) > 0) {
        if ((f->gtccr1 >> 4) != REFCLKIO) {
            LOG_W("GTCCR1 = 0x%x", f->gtccr1);
            goto fault;
        }
    } else {
        if ((f->gtccr1 >> 4) != MCLK) {
            LOG_W("GTCCR1 = 0x%x", f->gtccr1);
            goto fault;
        }
    }

	LOG_I("DS26518 TEST OK!");
	return;

fault:
	LOG_E("DS26518 TEST FAILED!");
}

void ds26518_monitor_tx_slot(int e1_no, int slot)
{
	FRAMER *f = ds26518_framer(e1_no);
	f->tds0sel = slot & 0x1f;
}

void ds26518_monitor_rx_slot(int e1_no, int slot)
{
	FRAMER *f = ds26518_framer(e1_no);
	f->rdsosel = slot & 0x1f;
}

u8_t ds26518_read_monitor_tx_slot(int e1_no)
{
	FRAMER *f = ds26518_framer(e1_no);
	return f->tdsom;
}

u8_t ds26518_read_monitor_rx_slot(int e1_no)
{
	FRAMER *f = ds26518_framer(e1_no);
	return f->rdsom;
}

#if 0
void ds26518_mon_test2(int e1_no, int slot)
{
    FRAMER *f = ds26518_framer(e1_no);
	HAL_Delay(1);
	ds26518_monitor_tx_slot(e1_no, slot);
    ds26518_monitor_rx_slot(e1_no, slot);

	LOG_I("Now in monitor test2, print E1 '%d' slot '%d' tx, rx data", e1_no, slot);
    print_zl50020_cml_value(0, slot);
    for(int i = 0; i < 10; i++){
        LOG_D("tx_data = %x, rx_data = %x", f->tdsom, f->rdsom);
    }
    
	read_zl50020_data_mem(0, slot);
}
#endif

void ds26518_display_rx_data(int e1_no, int slot)
{
    FRAMER *f = ds26518_framer(e1_no);
    //u8_t tx_data[300] = {0};
    u8_t rx_data[300] = {0};
    u8_t dummy;

    LOG_I("Now print '%d' e1 '%d' slot rx memory!", e1_no, slot);
    
    for(int i = 0; i < 300; i++) {
        //tx_data[i] = f->tdsom;
        rx_data[i] = f->rdsom;
        for (int j = 0; j < 20*16; j++) {
            dummy = f->tdsom;
            if (dummy != 0x86) {
                continue;
            }
        }
    }
    //LOG_HEX("tx-data", 16, tx_data, 300);
    LOG_HEX("rx-data", 16, rx_data, 300);
}
void ds26518_monitor_test(int e1_no, int slot)
{
	u8_t test_value ;
    FRAMER *f = ds26518_framer(e1_no);
    ds26518_port_init(e1_no, CCS_TYPE);
    ds26518_monitor_tx_slot(e1_no, slot);
    ds26518_monitor_rx_slot(e1_no, slot);
	ds26518_e1_slot_enable(e1_no,slot,VOICE_ACTIVE);
    
    set_ds26518_loopback(e1_no, FRAME_LOCAL_LP);
    HAL_Delay(10);
    
    LOG_I("Now loopback, read some data ");
    ds26518_display_rx_data(e1_no, slot);
    
	if (slot == 0) {
		test_value = 0x9b;
	}else {
		test_value = 0x7f;
	}
	read_zl50020_data_mem(e1_no, slot, test_value);
    
    set_ds26518_loopback(e1_no, NO_LP);
}

#if 0
void ds26518_zl50020_test(int e1_no, int slot_in, int slot_out)
{
   
    FRAMER *f = ds26518_framer(e1_no);
    ds26518_port_init(e1_no, CCS_TYPE);
    ds26518_monitor_tx_slot(e1_no, slot_in);
    ds26518_monitor_rx_slot(e1_no, slot_in);
    
    set_ds26518_loopback(e1_no, FRAME_LOCAL_LP);
    
    ds26518_e1_slot_enable(e1_no,slot_in,VOICE_ACTIVE); //tx, rx
    
    HAL_Delay(10);
    LOG_I("Now print '%d' e1 '%d' slot rx memory!", e1_no, slot_in);
    ds26518_display_rx_data(e1_no, slot_in);
    
    HAL_Delay(10);
    read_zl50020_data_mem(e1_no, slot_in);
    
    connect_slot(slot_in,e1_no, slot_out, e1_no);
    HAL_Delay(10);
    ds26518_display_rx_data(e1_no, slot_in);
    read_zl50020_data_mem(e1_no, slot_in);
}
#endif

void ds26518_BERT_error_insert_rate(int e1_no, int err_rate)
{
	BERT *bert = ds26518_bert(e1_no);

	/* 0: No errors automatically inserted 
	*  1: 10E-1
	*  2: 10E-2
	*  3: 10E-3
	*  4-7: 10E-4 ~~ 10E-7
	*/
	bert->bc2 = BC2_EIB(err_rate & 7);
}

void ds26518_enable_bert(int e1_no, int pattern)
{
	FRAMER *f = ds26518_framer(e1_no);
	BERT *bert = ds26518_bert(e1_no);

    e1_port_init(e1_no);
	/* System (backplane) operation. Rx BERT port receives data from the transmit path. The transmit path enters the receive BERT on the line side of the elastic store (if enabled). */
	f->rxpc = RXPC_RBPEN | RXPC_RPPDIR; 

	/* System (backplane) operation. Transmit BERT port sources data into the receive path (RSERn). In this mode, the data from the BERT is muxed into the receive path. */
	f->txpc = TXPC_TBPEN | TXPC_TBPDIR;

	f->rbpcs[0] = 0x2; // enable slot 1 (from 0 index);
	f->tbpcs[0] = 0x2; // enable slot 1
    
    connect_bert_slot(e1_no, 1);

	ds26518_BERT_error_insert_rate(e1_no, 0);
	/* 0: Pseudorandom 2E7–1
	*  1: Pseudorandom 2E11–1
	*  2: Pseudorandom 2E15–1
	*  3: Pseudorandom Pattern QRSS. A 2 20- 1 pattern with 14 consecutive zero restriction
	*  4: Repetitive Pattern
	*  5: Alternating Word Pattern
	*  6: Modified 55 Octet (Daly) Pattern
	*  7: Pseudorandom 2E-9-1
	*/
	bert->bc1 = BC1_PS(pattern & 7);

	if (pattern == 4) {
		bert->brp[0] = 0xad;
		bert->brp[1] = 0xb5;
		bert->brp[2] = 0xd6;
		bert->brp[3] = 0x5a;
	} else {
		memset((void *)(bert->brp), 0xff, 4);
	}

	/* start tc */
    HAL_Delay(1);
	bert->bc1 |= (BC1_TC | BC1_LC);
}

void ds26518_bert_report(int e1_no)
{
    EXTBERT *ebert = ds26518_exbert(e1_no);
	BERT *bert = ds26518_bert(e1_no);
    
    LOG_W("BERT Status Register = %X, Real Status = %X", bert->bsr, ebert->brsr);
    LOG_W("BERT Latched Status = %X/%X", ebert->blsr1, ebert->blsr2);
    LOG_W("BERT Bit Counter = %d", (u32_t)bert->bbc[0] | (u32_t)bert->bbc[1] << 8 | (u32_t)bert->bbc[2] << 16 | (u32_t)bert->bbc[3] << 24);
    LOG_W("BERT Bit Error = %d", (u32_t)bert->bec[0] | (u32_t)bert->bec[1] << 8 | (u32_t)bert->bec[2] << 16);
}

void ds26518_bert_test(int e1_no, int slot, int system_direction)
{
	FRAMER *f = ds26518_framer(e1_no & 7);
    BERT *bert = ds26518_bert(e1_no & 7);
    EXTBERT *ebert = ds26518_exbert(e1_no & 7);

	f->txpc = TXPC_TBPEN | (system_direction == 1 ? TXPC_TBPDIR : 0);
	f->rxpc = RXPC_RBPEN | (system_direction == 1 ? RXPC_RPPDIR : 0);

	int index = (slot & 0x1f) >> 3;
	int bit = (slot & 7);

	memset((void *)f->tbpcs, 0, 4);
	f->tbpcs[index] = 1 << bit;

	memset((void *)f->rbpcs, 0, 4);
	f->rbpcs[index] = 1 << bit;

	/* Alternating Word Pattern  = 5*/
	bert->bc1 = BC1_PS(5);
	bert->brp[0] = bert->brp[1] = 0;
	bert->brp[2] = bert->brp[3] = 0x7e;
	bert->bawc = 100;

	/* Connect slot */
	if (system_direction == 1) {
		connect_slot(slot, e1_no, slot, e1_no);
	} else {
		set_ds26518_loopback(e1_no, FRAME_LOCAL_LP);
	}
	
	/*Load pattern */
	bert->bc1 |= (BC1_TC | BC1_LC);

	HAL_Delay(2);

	bert->bc1 &= ~(BC1_LC);
	bert->bc1 |= BC1_LC;

	HAL_Delay(2);

	u32_t bit_counts = ((u32_t)bert->bbc[3] << 24) | ((u32_t)bert->bbc[2] << 16) | ((u32_t)bert->bbc[1] << 8) | bert->bbc[0];
	u32_t bit_err_counts = ((u32_t)bert->bec[2] << 16) | ((u32_t)bert->bec[1] << 8) | bert->bec[0];

	u8_t real_status = ebert->brsr;
	u8_t latch_status1 = ebert->blsr1;
	u8_t latch_status2 = ebert->blsr2;

	ebert->blsr1 = latch_status1;
	ebert->blsr2 = latch_status2;

	LOG_I("--------------E1 %d Slot %d BERT TEST RESULT-----------------", e1_no, slot);
	LOG_I("Bit counts\tBit ERROR counts");
	LOG_I("%d\t%d", bit_counts, bit_err_counts);
	LOG_I("Real_status\tla status1\tla status2");
	LOG_I("%02x\t%02x\t%02x", real_status, latch_status1, latch_status2);
	LOG_I("---------------END REPROT-------------------------------------");
}

char *frame_status(u8_t status)
{
	if (status & RRTS7_CRC4SA) {
		return "CRC4 Multiframer";
	} else if (status & RRTS7_CASSA) {
		return "CAS Multiframer";
	} else if (status & RRTS7_FASSA) {
		return "Framer";
	} else {
		return "NOT Aligned!";
	}
}

void ds26518_frame_status(int e1_no)
{
    FRAMER *f = ds26518_framer(e1_no);

    LOG_I("DS2658 e1 '%d' frame status = %x:'%s'", e1_no,f->rfdl_rrts7, frame_status(f->rfdl_rrts7));


	//out_tx_abcd(0, 17, 7);
	//HAL_Delay(2);


	u8_t read_abcd[16];
	for (int i = 0; i < 16; i++) {
		read_abcd[i] = f->rs[i];
	}
	LOG_HEX("rx-abcd", 16, read_abcd, 16);

#if 0
	ds26518_monitor_tx_slot(e1_no, 16);
    ds26518_monitor_rx_slot(e1_no, 16);

    LOG_I("Now print E1 '%d' slot '%d' tx, rx data", e1_no, 16);
    
    for(int i = 0; i < 20; i++){
        LOG_D("tx_data = %x, rx_data = %x", f->tdsom, f->rdsom);
    }
#endif
}

