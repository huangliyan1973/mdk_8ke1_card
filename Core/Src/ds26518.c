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
#include "8ke1_debug.h"
#include "mtp.h"
#include "lwip/sys.h"

/* FSMC_NE2 for ds26518 */
#define DS26518_BASE			(0x64000000)
#define DS26518_DEVICE			(DEVICE *)DS26518_BASE

#define MTP_DEBUG                CARD_DBG_ON

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

static HDLC *ds26518_hdlc(int e1_no)
{
	DEVICE *dev = DS26518_DEVICE;
	return (HDLC *)&dev->hdlc[e1_no];
}

void set_ds26518_interrupt(int e1_no, int enable)
{
	FRAMER *f = ds26518_global_framer();

	if (enable) {
		f->gfimr1 |= (1 << e1_no);
		f->glimr1 |= (1 << e1_no);
	} else {
		f->gfimr1 &= ~(1 << e1_no);
		f->glimr1 &= ~(1 << e1_no);
	}
}

void set_ds26518_global_interrupt(int enable)
{
	FRAMER *f = ds26518_global_framer();
	if (enable) {
		f->gtcr1 &= ~GTCR1_GIPI;
	} else {
		f->gtcr1 |= GTCR1_GIPI;
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

void ds26518_e1_slot_enable(int e1_no, int slot, enum SLOT_ACTIVE active)
{
	FRAMER *f = ds26518_framer(e1_no);

	if (slot == 0 || slot == 16) {
		return;
	}

	uint32_t *val = (uint32_t *)&(f->tcice[0]);
	uint32_t mask = (uint32_t)(1L << (slot & 0x1F));

	if (active == VOICE_ACTIVE) {
		*val = (*val) & (~mask);
	} else {
		*val = (*val) | mask;
	}
	CARD_DEBUGF(MTP_DEBUG, ("tcice value = %04X\n", *val));
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
	set_ds26518_tclk_src(TCLK_PIN);

	f->gtcr3 = 0x3;
	f->gtccr3 = GTCCR3_RSYSCLKSEL | GTCCR3_TSYSCLKSEL ;
}

void set_ds26518_slave_clock(void)
{
	FRAMER *f = ds26518_global_framer();

	set_ds26518_backplane_refclock(REFCLKIO); //???
	set_ds26518_tclk_src(TCLK_PIN);

	f->gtcr3 = 0x1;
	f->gtccr3 = 0;
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
	f->gtccr1 = 0x80; //2.048MHz derived from MCLK. (REFCLKIO is an output.)

	dummy = f->idr;		/* delay at least 300 ns */
	dummy = f->idr;

	CARD_DEBUGF(MTP_DEBUG,("DS26518 ID=%"X8_F"\n", dummy));

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
	/* GTCR1
	 * RLOF output,
	 * 528MD enabled,
	 * disable global interrupt
	 * use internal ibo mux
	 * */
	f->gtcr1 = GTCR1_GPSEL(0) | GTCR1_GIPI ; //| GTCR1_528MD;

	/* GFCR1
	 * BIT7-BIT6 = 3: IBO MODE: 16.384M (8 devices on TSER1 and RSER1)
	 * BIT5-BIT4 = 3: BPCLK1: 	 16.384M CLK OUTPUT.
	 * RMSYNC/RFSYNC[8:1] pins output RFSYNC[8:1](Receive Frame Sync)
	 * TCHBLK/TCHCLK[8:1] pins output TCHBLK[8:1] (Transmit Channel Block)
	 * RCHBLK/RCHCLK[8:1] pins output RCHBLK[8:1] (Receive Channel Block)
	 */
	f->gfcr1 = GFCR1_IBOMS(3) | GFCR1_BPCLK(3) ;

	/* GTCR3
	 * Bit 1 = 0, TSSYNCIOn are Input PIN.
	 * 		 = 1, TSSYNCIOn are Output sync to BPCLK1.
	 * Bit 0 = 0, TSYNCn is selected for TSYNC/TSSYNCIO[8:1] pins
	 *       = 1, TSSYNCIOn is selected for TSYNC/TSSYNCIO[8:1] pins.
	 */
	f->gtcr3 = 0x3;

	/*GTCCR3
	 * BIT6 = 1 Use BPCLK1 as the master clock for all eight receive system clocks (Channels 1–8).
	 * BIT5 = 1 Use BPCLK1 as the master clock for all eight transmit system clocks (Channels 1–8).
	 * BIT4 = 0: Use TCLKn pins for each of the transmit clock (Channels 1–8).
	 * 		= 1: Use REFCLKIO as the master clock for all eight transmit clocks (Channels 1–8).
	 */
	f->gtccr3 = GTCCR3_RSYSCLKSEL | GTCCR3_TSYSCLKSEL ;

	set_ds26518_global_interrupt(0);

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
	if (sig_type == CAS_TYPE) {
		f->tcr1 |= TCR1_T16S;
	}

	/* TIBOC
	 * Bit 4 = 1, Frame Interleave.
	 * Bit 3 = 1, Interleave Bus Operation enabled.
	 */
	f->tiboc = TIBOC_IBOSEL | TIBOC_IBOEN;

	/* E1AF, MUST be 0x1b */
	f->tslc1_taf = 0x1b;

	/* E1NAF, MUST be 0x40 */
	f->tslc2_tnaf = 0x40;

	/* TIOCR
	 * Bit7 = 0, TCLKn No inversion.
	 * Bit6 = 0, TSYNCn No inversion.
	 * Bit5 = 0, TSSYNCIOn No inversion.
	 * Bit4 = 1, If TSYSCLKn is 2.048/4.096/8.192MHz or IBO enabled.
	 * Bit3 = 0, TSSYNCIOn Mode Select Frame mode.
	 * Bit2 = 0, TSYNCn I/O Select input.
	 * Bit0 = 0, TSYNCn Mode Select Frame mode.
	 */
	f->tiocr = TIOCR_TSCLKM;

	/* Transmit Elastic store is enabled */
	f->tescr = TESCR_TESE;

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
	memset((void *)f->ssie, 0, 4);

	/* The Transmit Idle Code Definition Registers */
	memset((void *)f->tidr, 0xD5, 32);

	/* The Transmit Channel Idle Code Enable registers */
	ds26518_tcice_init(e1_no);

	//f->ts[0] = 0xB; // 0 slot send bit.

	f->tsacr = 0x0;

	/* 设置接收端参数 */
	f->rmmr |= RMMR_FRM_EN | RMMR_T1E1 | RMMR_DRSS;

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
	f->riboc = RIBOC_IBOSEL | RIBOC_IBOEN;


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

	liu->lmcr &= 0xfe;
}

void enable_e1_transmit(int e1_no)
{
	LIU	*liu = ds26518_liu(e1_no);

	liu->lmcr |= 0x1;
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

void ds26519_isr(void)
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

		}
	}
}

#if 0
void ds26518_isr(void)
{
	LIU	*liu;

	FRAMER *f = ds26518_global_framer();

	//uint8_t frameISRStatus = f->gfisr1; //指示哪个FRAME发生中断，每一位表示一个FRAMER（0-7）
	//uint8_t BERTISRStatus = f->gbisr1; //BERT中断指示，每一位表示一个FRAMER（0-7）
	//uint8_t LiuISRStatus = f->glisr1; //LIU中断指示

	//根据前面读取的状态标志确定chl通道号
	uint8_t chl = 0; // chl 表示具体的通道号，chl = 0 - 7;

	f = ds26518_framer(chl);
	uint8_t tls_status = f->tiir; //Transmit Latched Status Register(TLS)1-3
	if (tls_status & 1){//TLS1 interrupt actived.
		uint8_t tls1 = f->tls1;
		if (tls1 & 1){
			vLog(MONITOR_DEBUG,"Loss of Transmit Clock Condition on %d LINK,Check TCLKn pin!\r\n", chl);
		}
		if (tls1 & 2){
			vLog(MONITOR_DEBUG,"Loss of Transmit Clock Condition Cleard on %d LINK\r\n", chl);
		}
		if (tls1 & 4 ){
			vLog(MONITOR_DEBUG,"Set every 2ms on transmit multiframe boundaries on %d LINK\r\n", chl);
			//Used to alert the host that signaling data needs to be updated
			//可以作为2ms的定时器
		}
		if (tls1 & 0x20){
			vLog(MONITOR_DEBUG,"Transmit Elastic Store Slip Occurrence Event on %d LINK\r\n", chl);
		}
		if (tls1 & 0x40){
			vLog(MONITOR_DEBUG,"Transmit Elastic Store Empty Event on %d LINK\r\n", chl);
		}
		if (tls1 & 0x80){
			vLog(MONITOR_ERR,"Transmit Elastic Store FULL Event on %d LINK\r\n", chl);
		}
		f->tls1 = tls1;
	}
	if (tls_status & 2){ //TLS2
		//for HDLC64....
		uint8_t tls2 = f->tls2;
		if (tls2 & 1){
			vLog(MONITOR_DEBUG,"Transmit FIFO Not Full Set Condition (TNFS) on %d LINK\r\n", chl);
		}
		if (tls2 & 2){
			vLog(MONITOR_DEBUG,"Transmit FIFO Below Low Watermark Set Condition (TLWMS) on %d LINK\r\n", chl);
			if (msg_len > 0){ //need to send.
				uint8_t count = f->tfba;
				uint8_t i;
				if (msg_len > count){
					for(i = 0; i < count; i++){
						f->thf = msg_buf[i];
					}
					msg_len -= count;

					for(i = 0; i < msg_len; i++){
						msg_buf[i] = msg_buf[i + count];
					}
					vLog(MONITOR_WARN, "Already send %d byte, NOT END...\r\n",count);
				}else{
					for(i = 0; i < msg_len-1; i++){
						f->thf = msg_buf[i];
					}
					f->thc1 |= THC1_TEOM; // bit2 of thc1 TEOM
					f->thf = msg_buf[msg_len -1];
					vLog(MONITOR_WARN, "Already send %d byte,Waite a interrupt end.\r\n",msg_len);
					msg_len =  0;
					f->tim2 |= TIM2_TMEND;
				}
			}
		}
		if (tls2 & 4 ){
			vLog(MONITOR_DEBUG,"Transmit Message End Event (TMEND) on %d LINK\r\n", chl);
			f->tim2 &= (~TIM2_TMEND);
		}
		if (tls2 & 8 ){
			vLog(MONITOR_DEBUG,"Transmit FIFO Underrun Event (TUDR) on %d LINK\r\n", chl);
		}
		f->tls2 = tls2;
	}
	if (tls_status & 4){ //TLS3
		uint8_t tls3 = f->tls3;
		if (tls3  & 1){
			vLog(MONITOR_DEBUG,"Loss Of Frame (LOF) on %d LINK\r\n", chl);
		}
		if (tls3 & 2){
			vLog(MONITOR_DEBUG,"Loss Of Frame Synchronization Detect (LOFD) on %d LINK\r\n", chl);
		}
		f->tls3 = tls3;
	}

	uint8_t rls_status = f->riir; //RLS1,3,4,5,7
	if (rls_status & 1){ //RLS1
		uint8_t rls1 = f->rls1;
		if (rls1 & 1){
			vLog(MONITOR_DEBUG,"Receive Loss of Frame Condition Detect (RLOFD) on %d LINK\r\n", chl);
		}
		if (rls1 & 2){
			vLog(MONITOR_DEBUG,"Receive Loss of Signal Condition Detect (RLOSD) on %d LINK\r\n", chl);
		}
		if (rls1 & 4){
			vLog(MONITOR_DEBUG,"Receive Alarm Indication Signal Condition Detect (RAISD) on %d LINK\r\n", chl);
		}
		if (rls1 & 8){
			vLog(MONITOR_DEBUG,"Receive Remote Alarm Indication Condition Detect (RRAID) on %d LINK\r\n", chl);
		}
		if (rls1 & 0x10){
			vLog(MONITOR_DEBUG,"Receive Loss of Frame Condition Clear (RLOFC) on %d LINK\r\n", chl);
		}
		if (rls1 & 0x20){
			vLog(MONITOR_DEBUG,"Receive Loss of Signal Condition Clear (RLOSC) on %d LINK\r\n", chl);
		}
		if (rls1 & 0x40){
			vLog(MONITOR_DEBUG,"Receive Alarm Indication Signal Condition Clear (RAISC) on %d LINK\r\n", chl);
		}
		if (rls1 & 0x80){
			vLog(MONITOR_DEBUG,"Receive Remote Alarm Indication Condition Clear (RRAIC) on %d LINK\r\n", chl);
		}
		f->rls1 = rls1;
	}
	if (rls_status & 2){ //RLS2 for E1
		uint8_t rls2 = f->rls2;
		if (rls2 & 1){
			vLog(MONITOR_DEBUG,"Receive Align Frame Event (RAF) every 250us on %d LINK\r\n", chl);
		}
		if (rls2 & 2){
			vLog(MONITOR_DEBUG,"Receive CRC-4 Multiframe Event (RCMF) every 2ms on %d LINK\r\n", chl);
		}
		if (rls2 & 4){
			vLog(MONITOR_DEBUG,"Receive Signaling All Zeros Event (RSA0) on %d LINK\r\n", chl);
		}
		if (rls2 & 8){
			//enabled in CCS mode.the contents of time slot 16 contains fewer than three zeros over 16 consecutive frames
			vLog(MONITOR_DEBUG,"Receive Signaling All Ones Event (RSA1) on %d LINK\r\n", chl);
		}
		if (rls2 & 0x10){
			vLog(MONITOR_DEBUG,"FAS Resync Criteria Met Event (FASRC) on %d LINK\r\n", chl);
		}
		if (rls2 & 0x20){
			vLog(MONITOR_DEBUG,"CAS Resync Criteria Met Event (CASRC) on %d LINK\r\n", chl);
		}
		if (rls2 & 0x40){
			vLog(MONITOR_DEBUG,"CRC Resync Criteria Met Event (CRCRC) on %d LINK\r\n", chl);
		}
		f->rls2 = rls2;
	}
	if (rls_status & 4){ //RLS3 for E1
		uint8_t rls3 = f->rls3;
		if (rls3 & 1){
			vLog(MONITOR_DEBUG,"Receive Distant MF Alarm Detect (RDMAD) on %d LINK\r\n", chl);
		}
		if (rls3 & 2){
			vLog(MONITOR_DEBUG,"V5.2 Link Detect (V52LNKD) on %d LINK\r\n", chl);
		}
		if (rls3 & 8){
			vLog(MONITOR_DEBUG,"Loss of Receive Clock Detect (LORCD) on %d LINK\r\n", chl);
		}
		if (rls3 & 0x10){
			vLog(MONITOR_DEBUG,"Receive Distant MF Alarm Clear (RDMAC) on %d LINK\r\n", chl);
		}
		if (rls3 & 0x20){
			vLog(MONITOR_DEBUG,"V5.2 Link Detected Clear (V52LNKC) on %d LINK\r\n", chl);
		}
		if (rls3 & 0x80){
			vLog(MONITOR_DEBUG,"Loss of Receive Clock CLEAR (LORCC) on %d LINK\r\n", chl);
		}
		f->rls3 = rls3;
	}
	if (rls_status & 8){ //RLS4
		uint8_t rls4 = f->rls4;
		if (rls4 & 1){
			vLog(MONITOR_DEBUG,"Receive Multiframe Event (RMF) every 2ms on %d LINK\r\n", chl);
		}
		if (rls4 & 2){
			vLog(MONITOR_DEBUG,"Timer Event (TIMER) Set on increments of 1 second or 62.5ms based on RCLKn on %d LINK\r\n", chl);
		}
		if (rls4 & 4){
			vLog(MONITOR_DEBUG,"One-Second Timer (1SEC) on %d LINK\r\n", chl);
		}
		if (rls4 & 8){
			//Signaling Change of State Interrupt Enable registers (RSCSE1 through RSCSE3) changes signaling state.
			vLog(MONITOR_DEBUG,"Receive Signaling Change of State Event (RSCOS) on %d LINK\r\n", chl);
		}
		if (rls4 & 0x20){
			vLog(MONITOR_DEBUG,"Receive Elastic Store Slip Occurrence Event (RSLIP) on %d LINK\r\n", chl);
		}
		if (rls4 & 0x40){
			vLog(MONITOR_DEBUG,"Receive Elastic Store Empty Event (RESEM) on %d LINK\r\n", chl);
		}
		if (rls4 & 0x80){
			vLog(MONITOR_ERR,"Receive Elastic Store Full Event (RESF) on %d LINK\r\n", chl);
		}
		f->rls4 = rls4;
	}

	if (rls_status & 0x10){ //RLS5
		//For HDLC-64 status
		uint8_t rls5 = f->rls5;
		if (rls5 & 1){
			vLog(MONITOR_DEBUG,"Receive FIFO Not Empty Set Event (RNES) on %d LINK\r\n", chl);
		}
		if (rls5 & 2){
			vLog(MONITOR_DEBUG,"Receive FIFO Above High Watermark Set Event (RHWMS) on %d LINK\r\n", chl);
		}
		if (rls5 & 4){
			vLog(MONITOR_DEBUG,"Receive Packet Start Event (RPS) on %d LINK\r\n", chl);
		}
		if (rls5 & 8){
			//Signaling Change of State Interrupt Enable registers (RSCSE1 through RSCSE3) changes signaling state.
			vLog(MONITOR_DEBUG,"Receive Packet End Event (RPE) on %d LINK\r\n", chl);
		}
		if (rls5 & 0x10){
			vLog(MONITOR_DEBUG,"Receive HDLC-64 Opening Byte Event (RHOBT) on %d LINK\r\n", chl);
		}
		if (rls5 & 0x20){
			vLog(MONITOR_DEBUG,"Receive FIFO Overrun (ROVR) on %d LINK\r\n", chl);
		}
		f->rls5 = rls5;
	}
	if (rls_status & 0x40){ //RLS7
		//Not used...
	}

	liu = ds26518_liu(chl);

	uint8_t llsr = liu->llsr; /* Bit6: Open-Circuit Clear (OCC). This latched bit is set when an open circuit condition was detected at TTIPn and TRINGn and then removed
						Bit5: Short-Circuit Clear (SCC). This latched bit is set when a short circuit condition was detected at TTIPn and TRINGn and then removed
						Bit4: Loss of Signal Clear (LOSC). This latched bit is set when a loss of signal condition was detected at RTIPn and RRINGn and then removed.
						Bit 2: Open-Circuit Detect (OCD). This latched bit is set when open-circuit condition is detected at TTIPn and TRINGn
						Bit 1: Short-Circuit Detect (SCD). This latched bit is set when short-circuit condition is detected at TTIPn and TRINGn
						Bit 0: Loss of Signal Detect (LOSD). This latched bit is set when an LOS condition is detected at RTIPn and RRINGn  */
	if (llsr & 1){
		vLog(MONITOR_ERR,"Loss of Signal Detect (LOSD) on %d LINK\r\n", chl);
	}
	if (llsr & 2){
		vLog(MONITOR_ERR,"Short-Circuit Detect (SCD) on %d LINK\r\n", chl);
	}
	if (llsr & 4){
		vLog(MONITOR_ERR,"Open-Circuit Detect (OCD) on %d LINK\r\n", chl);
	}
	if (llsr & 0x10){
		vLog(MONITOR_DEBUG,"Loss of Signal Clear (LOSC) on %d LINK\r\n", chl);
	}
	if (llsr & 0x20){
		vLog(MONITOR_DEBUG,"Short-Circuit Clear (SCC) on %d LINK\r\n", chl);
	}
	if (llsr & 0x40){
		vLog(MONITOR_DEBUG,"Open-Circuit Clear (OCC) on %d LINK\r\n", chl);
	}

	liu->llsr = llsr;
	//uint8_t bsr = (BERT *)(&dev->bert[chl])->bsr; //误码检测相关设置，暂时不用

}
#endif

#if 0
/*
 * direction = 0 : 出方向设置IDLE Code transmit
 * 			 = 1 :   入方向设置IDLE Code  receive
 * e1_no  = 0-7
 * slot = 0-31
 */
void insert_idle_slot(int direction, uint8_t e1_no, uint8_t slot)
{
	volatile uint8_t *pEnable, *pData;

	f = (FRAMER *)(&dev->te1[e1_no]);
	if(direction == 0){ //设置出方向时隙为IDLE Code
		pEnable = f->tcice;
		pData = f->tidr;
	}else{
		pEnable = f->rcice;
		pData = f->ridr;
	}

	*(pEnable  + (slot << 3)) |=  1 << (slot & 7);

	*(pData + slot) = 0x55; //IDLE Code for uLaw.
}

void insert_normal_slot(int direction, uint8_t e1_no, uint8_t slot)
{

	volatile uint8_t  *pEnable;

	f = (FRAMER *)(&dev->te1[e1_no]);
	if(direction == 0){ //设置出方向时隙为IDLE Code
		pEnable = f->tcice;
	}else{
		pEnable = f->rcice;
	}

	*(pEnable  + (slot << 3)) &=  ~(1 << (slot & 7));
}
#endif

