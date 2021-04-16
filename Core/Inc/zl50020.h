/*
 * zl50020.h
 *
 *  Created on: 2020年12月16日
 *      Author: hly66
 */

#ifndef INC_ZL50020_H_
#define INC_ZL50020_H_

#include <stdint.h>
#include "lwip/sys.h"

/* STM32F407 PC7 pin connect with ZL50020 DTA_RDY pin */

/* STM32F407 PF7-PF10 LED0,LED1 CONTROL */

#define LED1_GREEN_ON	do {HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_SET); \
							HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_RESET); } while(0)

#define LED1_RED_ON		do {HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_SET); \
							HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_RESET); } while(0)

#define LED1_ORG_ON		do {HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_SET); \
							HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_SET);} while(0)

#define LED2_GREEN_ON	do {HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET); \
							HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET);} while(0)

#define LED2_RED_ON		do {HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET); \
							HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET);} while(0)

#define LED2_ORG_ON		do {HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_SET); \
							HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_SET);} while(0)

#define LED1_OFF        do {HAL_GPIO_WritePin(GPIOF, GPIO_PIN_7, GPIO_PIN_RESET); \
							HAL_GPIO_WritePin(GPIOF, GPIO_PIN_8, GPIO_PIN_RESET); } while(0)

#define LED2_OFF        do {HAL_GPIO_WritePin(GPIOF, GPIO_PIN_9, GPIO_PIN_RESET); \
							HAL_GPIO_WritePin(GPIOF, GPIO_PIN_10, GPIO_PIN_RESET);} while(0)


#define TONE_E1			7
#define TONE_SILENT 	31
#define CONF_E1		    6


#define TONE_STREAM 	8
#define MFC_STREAM 		9
#define CONF_STREAM 	10


typedef volatile uint16_t  vu16;

struct zl50020_dev {
	vu16	cr;  	/* 0 Control Register */
	vu16 	ims;	/* 1 Internal Mode Selection Register */
	vu16	srr;	/* 2 Software Reset Register */
	vu16	ocfcr;	/* 3 Output Clock and Frame Pulse Control Register */
	vu16	ocfsr;  /* 4 Output Clock and Frame Pulse Selection Register */
	vu16	fpoff0; /* 5 FPo_OFF0 Register */
	vu16	fpoff1; /* 6 FPo_OFF1 Register */
	vu16	fpoff2; /* 7 FPo_OFF2 Register */

	vu16	hole[8]; /* 0x8 - 0xF */

	vu16	ifr;	/* 0x10 Internal Flag Register */
	vu16	berfr0; /* 0x11 BER Error Flag Register 0 */
	vu16	berfr1; /* 0x12 BER Error Flag Register 1 */
	vu16	berlr0; /* 0x13 BER Receiver Lock Register 0 */
	vu16	berlr1; /* 0x14 BER Receiver Lock Register 1 */

	vu16	hole1[0x100 - 0x15]; /* 0x15 - 0xFF */

	vu16	sicr[32]; 	/* 0x100-0x11F Stream Input Control Registers 0 - 31 */
	vu16	siqfr[32]; 	/* 0x120-0x13F Stream Input Quadrant Frame Registers 0 - 31 */

	vu16	hole2[0x200 - 0x140]; /* 0x140 - 0x1FF */

	vu16	socr[32]; 	/* 0x200-0x21F Stream Output Control Registers 0 - 31 */

	vu16	hole3[0x300 - 0x220];

	vu16	brsr[32];	/* 0x300-0x31F BER Receiver Start Registers 0 - 31 */
	vu16	brlr[32];	/* 0x320-0x33F BER Receiver Length Registers 0 - 31  */
	vu16	brcr[32];	/* 0x340-0x35F BER Receiver Control Registers 0 - 31 */
	vu16	brer[32];	/* 0x360-0x37F BER Receiver Error Registers 0 - 31 */
}__attribute__ ((packed));

struct zl50020_cml {
	vu16	sto_connect[32][256];
}__attribute__ ((packed));

/* IMS */
#define PRBS_Receiver_Enable	(1 <<  5)
#define PRBS_Transmitter_Enable	(1 << 4)

/* BRCR */
#define Bit_Error_Rate_Counter_Clear	(1 << 1)
#define Bit_Error_Rate_Test_Start		(1 << 0)

void enable_prbs_function(u8_t stream_no);

void print_prbs_value(u8_t stream_no, int stop_flag);

extern void connect_slot(uint16_t o_ts, uint16_t o_e1, uint16_t i_ts, uint16_t i_e1);

extern void connect_tone(uint16_t o_ts, uint16_t o_e1, uint16_t i_ts);

extern void m34116_conf_connect(u8_t p, u8_t gaini, u8_t ai, u8_t gaino, u8_t ao, u8_t c, u8_t s, u8_t pt);

extern void m34116_disconnect(u8_t slot);

extern void conf_module_detect(void);

extern u8_t read_dtmf(u8_t slot);

extern void set_card_e1_led(void);

extern void zl50020_test(void);

extern void led_test(void);

extern void module_test(void);

extern void connect_bert_slot(u8_t e1_no, u8_t slot);

extern void send_msg(u16_t stream_no, u16_t slot, u8_t data);

extern void zl50020_clkout_test(void);

extern void zl50020_bitDelay(u8_t stream_no, u8_t bit_delay);

extern void zl50020_bitAdvancement(u8_t stream_no, u8_t bit_adv);

extern void zl50020_frac_bit_adv(u8_t stream_no);

extern void read_zl50020_data_mem(u8_t stream_no, u8_t slot, u8_t test_value);

extern void print_zl50020_cml_value(u16_t stream_no, u16_t slot);

extern void print_zl50020(u8_t stream_no, u8_t slot);

extern u8_t mfc_t32_zl50020_test(u8_t test_value);

extern void m34116_zl50020_test(u8_t slot);

#endif /* INC_ZL50020_H_ */
