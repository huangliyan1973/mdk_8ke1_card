/*
 * zl50020.h
 *
 *  Created on: 2020年12月16日
 *      Author: hly66
 */

#ifndef INC_ZL50020_H_
#define INC_ZL50020_H_

#include <stdint.h>

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
};



#endif /* INC_ZL50020_H_ */