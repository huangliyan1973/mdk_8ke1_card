#ifndef DS26518_H
#define DS26518_H

typedef volatile unsigned char VUC;
typedef unsigned char UC;

#define VUC_FIELD(a, b, c) (((a) & (0xff >> (8 - (c)))) << (b))

typedef struct {

struct _te1span {
/*************************************************************************
 * Per Port Section
 *************************************************************************
 */

/*************************************************************************
 * Framer Receive Side
 *************************************************************************
 */
	VUC e1rdmwe[4];	/* 0 - 003 */

	VUC hole[12]; 	/* 004 - 00F */

	VUC rhc;	/* 010 */

#define RHC_RCRCD	(1 << 7)
#define RHC_RHR		(1 << 6)
#define RHC_RHMS	(1 << 5)
#define RHC_RHCS(x)	VUC_FIELD(x, 0, 5)

	VUC rhbse;	/* 011 */

#define RHBSE_BSE(x)	VUC_FIELD(x, 0, 8)

	VUC rdsosel;	/* 012 */

#define RDS0SEL_RCM(x)	VUC_FIELD(x, 0, 5)

	VUC rsigc;	/* 013 */

#define RSIGC_RFSA1	(1 << 4)
#define RSIGC_RSFF	(1 << 2)
#define RSIGC_RSFE	(1 << 1)
#define RSIGC_RSIE	(1 << 0)

#define RSIGC_CASMS	(1 << 4)

	VUC rcr2;	/* 014 */

#define RCR2_RSLC96	(1 << 4)
#define RCR2_OOF(x)	VUC_FIELD(x, 2, 2)
#define RCR2_RAIIE	(1 << 1)
#define RCR2_RRAIS	(1 << 0)

	VUC rbocc;	/* 015 */

#define RBOCC_RBR	(1 << 7)
#define RBOCC_RBD(x)	VUC_FIELD(x, 4, 2)
#define RBOCC_RBF(x)	VUC_FIELD(x, 1, 2)

	VUC hole2[10];	/* 016 - 01F */

	VUC ridr[32];	/* 020 - 03FH */

	VUC rs[16];	/* 040 - 04F */

	VUC lcvcr1;	/* 050 */

#define LCVCR1_LCVC(x)	VUC_FIELD(x, 0, 8)

	VUC lcvcr2;	/* 051 */

#define LCVCR2_(x)	VUC_FIELD(x, 0, 8)

	VUC pcvcr1;	/* 052 */

#define PCVCR1_PCVC(x)	VUC_FIELD(x, 0, 8)

	VUC pcvcr2;	/* 053 */

#define PCVCR2_PCVC(x)	VUC_FIELD(x, 0, 8)

	VUC foscr1;	/* 054 */

#define FOSCR1_FOS(x)	VUC_FIELD(x, 0, 8)

	VUC foscr2;	/* 055 */

#define FOSCR2_FOS(x)	VUC_FIELD(x, 0, 8)

	VUC ebcr1;	/* 056 */

#define EBCR1_EB(x)	VUC_FIELD(x, 0, 8)

	VUC ebcr2;	/* 057 */

#define EBCR2_EB(x)	VUC(FIELD(x, 0, 8)

	VUC feacr1;	/* 058 */

#define FEACR1_FEACR(x)	VUC_FIELD(x, 0, 8)

	VUC feacr2;	/* 059 */

#define FEACR2_FEACR(x)	VUC_FIELD(x, 0, 8)

	VUC febcr1;	/* 05A */

#define FEBCR1_FEBCR(x)	VUC_FIELD(x, 0, 8)

	VUC febcr2;	/* 05B */

#define FEBCR2_FEBCR(x)	VUC_FIELD(x, 0, 8)

	VUC hole3[4];	/* 05C - 05F*/

	VUC rdsom; 	/* 060 */

#define RDSOM_B(x)	VUC_FIELD(x, 0, 8)

	VUC hole313; 	/* 061 */

	VUC rfdl_rrts7;	/* 062 */

#define RFDL_RFDL(x)	VUC_FIELD(x, 0, 8)

#define RRTS7_CSC(x)	VUC_FIELD(x, 3, 5)
#define RRTS7_CRC4SA	(1 << 2)
#define RRTS7_CASSA	(1 << 1)
#define RRTS7_FASSA	(1 << 0)

	VUC rboc;	/* 063 */

#define RBOC_RBOC(x)	VUC_FIELD(x, 0, 6)

	VUC rslc1_raf; 	/* 064 */

#define RSLC1_C(x)	VUC_FIELD(x, 0, 8)

#define RAF_SI		(1 << 7)
#define RAF_RESERVED	(0x1b)

	VUC rslc2_rnaf;  	/* 065 */

#define RSLC2_M(x)	VUC_FIELD(x, 6, 2)
#define RSLC2_S(x)	VUC_FIELD(x, 3, 3)
#define RSLC2_C(x)	VUC_FIELD(x, 0, 3)

#define RNAF_SI		(1 << 7)
#define RNAF_A		(1 << 5)
#define RNAF_SA4	(1 << 4)
#define RNAF_SA5	(1 << 3)
#define RNAF_SA6	(1 << 2)
#define RNAF_SA7	(1 << 1)
#define RNAF_SA8	(1 << 0)

	VUC rslc3_rsiaf; /* 066 */

#define RSLC3_S(x)	VUC_FIELD(x, 3, 4)
#define RSLC3_A(x)	VUC_FIELD(x, 1, 2)
#define RSLC3_M(x)	VUC_FIELD(x, 0, 1)

#define RSAIF_SIF(x)	VUC_FIELD(x, 0, 8)

	VUC rsinaf;	/* 067 */

#define RSINAF_SiF(x)	VUC_FIELD(x, 0, 8)

	VUC rra;	/* 068 */

#define RRA_RRAF(x)	VUC_FIELD(x, 0, 8)

	VUC rsa4;	/* 069 */

#define RSA4_RSA4F(x)	VUC_FIELD(x, 0, 8)

	VUC rsa5;	/* 06A */

#define RSA5_RSA5F(x)	VUC_FIELD(x, 0, 8)

	VUC rsa6;	/* 06B */

#define RSA6_RSA6F(x)	VUC_FIELD(x, 0, 8)

	VUC rsa7;	/* 06C */

#define RSA7_RSA7F(x)	VUC_FIELD(x, 0, 8)

	VUC rsa8;	/* 06D */

#define RSA8_RSA8F(x)	VUC_FIELD(x, 0, 8)

	VUC sabits;	/* 06E */

#define SABITS_SA(x)	VUC_FIELD(x, 0, 5)

	VUC sa6code;	/* 06F */

#define SA6CODE_SA6(x)	VUC_FIELD(x, 0, 4)

	VUC hole8[16]; 	/* 070 - 07F */

	VUC rmmr; 	/* 080 */

#define RMMR_FRM_EN	(1 << 7)
#define RMMR_INIT_DONE	(1 << 6)
#define RMMR_DRSS	(1 << 5)
#define RMMR_SFTRST	(1 << 1)
#define RMMR_T1E1	(1 << 0)

	VUC rcr1; /* 081 */

#define RCR1_SYNCT	(1 << 7)
#define RCR1_RB8ZS	(1 << 6)
#define RCR1_RFM	(1 << 5)
#define RCR1_ARC	(1 << 4)
#define RCR1_SYNCC	(1 << 3)
#define RCR1_RJC	(1 << 2)
#define RCR1_SYNCE	(1 << 1)
#define RCR1_RESYNC	(1 << 0)

#define RCR1_RHDB3	(1 << 6)
#define RCR1_RSIGM	(1 << 5)
#define RCR1_RG802	(1 << 4)
#define RCR1_RCRC4	(1 << 3)
#define RCR1_FRC	(1 << 2)
#define RCR1_SYNCE	(1 << 1)
#define RCR1_RESYNC	(1 << 0)

	VUC ribcc_rcr2; /* 082 */

#define RIBCC_RUP(x)	VUC_FIELD(x, 3, 3)
#define RIBCC_RDN(x)	VUC_FIELD(x, 0, 3)

#define RCR2_RLOSA	(1 << 0)

	VUC rcr3; /* 083 */

#define RCR3_UALAW	(1 << 6)
#define RCR3_RSERC	(1 << 5)
#define RCR3_BINV(x)	VUC_FIELD(x, 3, 2)
#define RCR3_PLB	(1 << 1)
#define RCR3_FLB	(1 << 0)

	VUC riocr;	/* 084 */

#define RIOCR_RCLKINV	(1 << 7)
#define RIOCR_RSYNCINV	(1 << 6)
#define RIOCR_H100EN	(1 << 5)
#define RIOCR_RSCLKM	(1 << 4)
#define RIOCR_RSMS	(1 << 3)
#define RIOCR_RSIO	(1 << 2)
#define RIOCR_RSMS2	(1 << 1)
#define RIOCR_RSMS1	(1 << 0)

	VUC rescr;	/* 085 */

#define RESCR_RDATFMT	(1 << 7)
#define RESCR_RGCLKEN	(1 << 6)
#define RESCR_RSZS	(1 << 4)
#define RESCR_RESALGN	(1 << 3)
#define RESCR_RESR	(1 << 2)
#define RESCR_RESMDM	(1 << 1)
#define RESCR_RESE	(1 << 0)

	VUC ercnt;	/* 086 */

#define ERCNT_1SECS	(1 << 7)
#define ERCNT_MCUS	(1 << 6)
#define ERCNT_MECU	(1 << 5)
#define ERCNT_ECUS	(1 << 4)
#define ERCNT_EAMS	(1 << 3)
#define ERCNT_FSBE	(1 << 2)
#define ERCNT_MOSCRF	(1 << 1)
#define ERCNT_LCVCRF	(1 << 0)

	VUC rhfc;	/* 087 */

#define RHFC_RFHWM(x)	VUC_FIELD(x, 0, 2)

	VUC riboc;	/* 088 */

#define RIBOC_IBOSEL	(1 << 4)
#define RIBOC_IBOEN	(1 << 3)

	VUC rscc;	/* 089 */

#define RSCC_RSC(x)	VUC_FIELD(x, 0, 3)

	VUC rxpc;	/* 08A */

#define RXPC_RHMS	(1 << 7)
#define RXPC_RHEN	(1 << 6)
#define RXPC_RPPDIR	(1 << 2)
#define RXPC_RBPFUS	(1 << 1)
#define RXPC_RBPEN	(1 << 0)

	VUC rbpbs;	/* 08B */

#define RBPBS_BPBSE(x)	VUC_FIELD(x, 0, 8)

	VUC hole9;	/* 08C */

	VUC rhbs;	/* 08D */

#define RHPBS_RHBSE(x)	VUC_FIELD(x, 0, 8)

	VUC hole319[2];	/* 08E - 08F */

	VUC rls1;	/* 090 */

#define RLS1_RRAIC	(1 << 7)
#define RLS1_RAISC	(1 << 6)
#define RLS1_RLOSC	(1 << 5)
#define RLS1_RLOFC	(1 << 4)
#define RLS1_RRAID	(1 << 3)
#define RLS1_RAISD	(1 << 2)
#define RLS1_RLOSD	(1 << 1)
#define RLS1_RLOFD	(1 << 0)

	VUC rls2;	/* 091 */

#define RLS2_RPDV	(1 << 7)
#define RLS2_COFA 	(1 << 5)
#define RLS2_8ZD 	(1 << 4)
#define RLS2_16ZD	(1 << 3)
#define RLS2_SEFE	(1 << 2)
#define RLS2_B8ZS	(1 << 1)
#define RLS2_FBE	(1 << 0)

#define RLS2_CRCRC	(1 << 6)
#define RLS2_CASRC	(1 << 5)
#define RLS2_FASRC	(1 << 4)
#define RLS2_RSA(x)	VUC_FIELD(x, 2, 2)
#define RLS2_RCMF	(1 << 1)
#define RLS2_RAF	(1 << 0)

	VUC rls3;	/* 092 */

#define RLS3_LORCC 	(1 << 7)
#define RLS3_LSPC	(1 << 6)
#define RLS3_LDNC 	(1 << 5)
#define RLS3_LUPC	(1 << 4)
#define RLS3_LORCD	(1 << 3)
#define RLS3_LSPD	(1 << 2)
#define RLS3_LDND	(1 << 1)
#define RLS3_LUPD	(1 << 0)

#define RLS3_V52LNKC	(1 << 5)
#define RLS3_RDMAC	(1 << 4)
#define RLS3_V52LNKD	(1 << 1)
#define RLS3_RDMAD	(1 << 0)

	VUC rls4;	/* 093 */

#define RLS4_RESF	(1 << 7)
#define RLS4_RESEM	(1 << 6)
#define RLS4_RSLIP	(1 << 5)
#define RLS4_RSCOS	(1 << 3)
#define RLS4_1SEC	(1 << 2)
#define RLS4_TIMER	(1 << 1)
#define RLS4_RMF	(1 << 0)

	VUC rls5;	/* 094 */

#define RLS5_ROVR	(1 << 5)
#define RLS5_RHOBT	(1 << 4)
#define RLS5_RPE	(1 << 3)
#define RLS5_RPS	(1 << 2)
#define RLS5_RHWMS	(1 << 1)
#define RLS5_RNES	(1 << 0)

	VUC hole10;	/* 095 */

	VUC rls7;	/* 096 */

#define RLS7_RRAICI	(1 << 5)
#define RLS7_RAISCI	(1 << 4)
#define RLS7_RSLC96	(1 << 3)
#define RLS7_RFDLF	(1 << 2)
#define RLS7_BC		(1 << 1)
#define RLS7_BD		(1 << 0)

#define RLS7_SA6CD	(1 << 1)
#define RLS7_SAXCD	(1 << 0)

	VUC hole11;	/* 097 */

	VUC rss[4];	/* 098 - 09B */

	VUC rspcd1;	/* 09C */

#define RSPCD1_C(x)	VUC_FIELD(x, 0, 8)

	VUC rspcd2;	/* 09D */

#define RSPCD2_C(x)	VUC_FIELD(x, 0, 8)

	VUC hole13;	/* 09E */

	VUC riir;	/* 09F */

#define RIIR_RLS(x)	VUC_FIELD(x, 0, 7)

	VUC rim1;	/* 0A0 */

#define RIM1_RRAIC	(1 << 7)
#define RIM1_RAISC	(1 << 6)
#define RIM1_RLOSC	(1 << 5)
#define RIM1_RLOFC	(1 << 4)
#define RIM1_RRAID	(1 << 3)
#define RIM1_RAISD	(1 << 2)
#define RIM1_RLOSD	(1 << 1)
#define RIM1_RLOFD	(1 << 0)

	VUC rim2;	/* 0A1 */

#define RIM2_RSA1	(1 << 3)
#define RIM2_RSA0	(1 << 2)
#define RIM2_RCMF	(1 << 1)
#define RIM2_RAF	(1 << 0)

	VUC rim3;	/* 0A2 */

#define RIM3_LORCC	(1 << 7)
#define RIM3_LSPC	(1 << 6)
#define RIM3_LDNC	(1 << 5)
#define RIM3_LUPC	(1 << 4)
#define RIM3_LORCD	(1 << 3)
#define RIM3_LSPD	(1 << 2)
#define RIM3_LDND	(1 << 1)
#define RIM3_LUPD	(1 << 0)

#define RIM3_V52LNKC	(1 << 5)
#define RIM3_RDMAC	(1 << 4)
#define RIM3_V52LNKD	(1 << 1)
#define RIM3_RDMAD	(1 << 0)

	VUC rim4;	/* 0A3 */

#define RIM4_RESF	(1 << 7)
#define RIM4_RESEM	(1 << 6)
#define RIM4_RSLIP	(1 << 5)
#define RIM4_RSCOS	(1 << 3)
#define RIM4_1SEC	(1 << 2)
#define RIM4_TIMER	(1 << 1)
#define RIM4_RMF	(1 << 0)

	VUC rim5;	/* 0A4 */

#define RIM5_ROVR	(1 << 5)
#define RIM5_RHOBT	(1 << 4)
#define RIM5_RPE	(1 << 3)
#define RIM5_RPS	(1 << 2)
#define RIM5_RHWMS	(1 << 1)
#define RIM5_RNES	(1 << 0)

	VUC hole15;	/* 0A5 */

	VUC rim7;	/* 0A6 */

#define RIM7_RRAICI	(1 << 5)
#define RIM7_RAISCI	(1 << 4)
#define RIM7_RSLC96	(1 << 3)
#define RIM7_RFDLF	(1 << 2)
#define RIM7_BC		(1 << 1)
#define RIM7_BD		(1 << 0)

#define RIM7_SA6CD	(1 << 1)
#define RIM7_SAXCD	(1 << 0)

	VUC hole16;	/* 0A7 */

	VUC rscse[4];	/* 0A8 - 0AB */

	VUC rupcd1;	/* 0AC */

#define RUPCD_C(x)	VUC_FIELD(x, 0, 8)

	VUC rupcd2;	/* OAD */

#define RUPCD2_C(x)	VUC_FIELD(x, 0, 8)

	VUC rdncd1;	/* 0AE */

#define RDNCD1_C(x)	VUC_FIELD(x, 0, 8)

	VUC rdncd2;	/* 0AF */

#define RDNCD2_C(x)	VUC_FIELD(x, 0, 8)

	VUC rrts1;	/* 0B0 */

#define RRTS1_RRAI	(1 << 3)
#define RRTS1_RAIS	(1 << 2)
#define RRTS1_RLOS	(1 << 1)
#define RRTS1_RLOF	(1 << 0)

	VUC hole18;	/* 0B1 */

	VUC rrts3;	/* 0B2 */

#define RRTS3_LORC	(1 << 3)
#define RRTS3_LSP	(1 << 2)
#define RRTS3_LDN	(1 << 1)
#define RRTS3_LUP	(1 << 0)

#define RRTS3_V52LNK	(1 << 1)
#define RRTS3_RDMA	(1 << 0)

	VUC hole19;	/* 0B3 */

	VUC rrts5;	/* 0B4 */

#define RRTS5_PS(x)	VUC_FIELD(x, 4, 3)
#define RRTS5_RHWM	(1 << 1)
#define RRTS5_RNE	(1 << 0)

	VUC rhpba;	/* 0B5 */

#define RHPBA_MS 	(1 << 7)
#define RHPBA_RPBA(x)	VUC_FIELD(x, 7, 0)

	VUC rhf;	/* 0B6 */

#define RHF_RHD(x)	VUC_FIELD(x, 0, 8)

	VUC hole20[9];	/* 0B7 - 0BF */

	VUC rbcs[4];	/* 0C0 - 0C3 */

	VUC rcbr[4];	/* 0C4 - 0C7 */

	VUC rsi[4];	/* 0C8 - 0CB */

	VUC rgccs[4];	/* OCC - 0CF */

	VUC rcice[4];	/* ODO - 0D3 */

	VUC rbpcs[4];	/* 0D4 - 0D7 */

	VUC hole22[4];	/* 0D8 - 0DB */

	VUC rhcs[4];	/* ODC - 0DF */

	VUC hole42[16];	/* 0E0 - 0EF */

/*************************************************************************
 * End Framer Receive Side
 *************************************************************************
 */

/*************************************************************************
 * Global Register Map Section
 *************************************************************************
 */
	VUC gtcr1; 	/* 00f0 */

#define GTCR1_GPSEL(x)	VUC_FIELD(x, 4, 4)
#define GTCR1_GIBO	(1 << 2)
#define GTCR1_GCLE	(1 << 1)
#define GTCR1_GIPI	(1 << 0)
#define GTCR1_528MD (1 << 3)

	VUC gfcr1;	/* 00f1 */

#define GFCR1_IBOMS(x)	VUC_FIELD(x, 6, 2)
#define GFCR1_BPCLK(x)	VUC_FIELD(x, 4, 2)
#define GFCR1_RFMSS	(1 << 2)
#define GFCR1_TCBCS	(1 << 1)
#define GFCR1_RCBCS	(1 << 0)

	VUC gtcr3;	/* 00f2 */

#define GTCR3_TSSYNIOSEL (1 << 1)
#define GTCR3_TSYNCSEL (1 << 0)

	VUC gtccr1;	/* 00f3 */

#define GTCCR1_BPREFSEL(x) VUC_FIELD(x, 4, 4)
#define GTCCR1_BPFREQSEL (1 << 3)
#define GTCCR1_FREQSEL	(1 << 2)
#define GTCCR1_MPS(x)	VUC_FIELD(x, 0, 2)

	VUC gtccr3;	/* 00f4 */

#define GTCCR3_RSYSCLKSEL	(1 << 6)
#define GTCCR3_TSYSCLKSEL	(1 << 5)
#define GTCCR3_TCLKSEL		(1 << 4)
#define GTCCR3_CLKOSEL(x)	VUC_FIELD(x, 0, 4)

	VUC ghisr;	/* 00f5 */

#define GHISR_HIS(x)	VUC_FIELD(x, 0, 4)

	VUC gsrr1;	/* 00f6 */

#define GFSRR1_H256RST	(1 << 3)
#define GFSRR1_LRST	(1 << 2)
#define GFSRR1_BRST	(1 << 1)
#define GFSRR1_FRST	(1 << 0)

	VUC ghimr;	/* 00f7 */

#define GHIMR_HIM(x)	VUC_FIELD(x, 0, 4)

	VUC idr;	/* 00f8 */

#define IDR_ID(x)	VUC_FIELD(x, 0, 8)

	VUC gfisr1;	/* 00f9 */

#define GFISR1_FIS(x)	VUC_FIELD(x, 0, 4)

	VUC gbisr1;	/* 00fa */

#define GBISR1_BIS(x)	VUC_FIELD(x, 0, 4)

	VUC glisr1;	/* 00fb */

#define GLISR1_LIS(x)	VUC_FIELD(x, 0, 4)

	VUC gfimr1;	/* 00fc */

#define GFIMR1_FIM(x)	VUC_FIELD(x, 0, 4)

	VUC gbimr1;	/* 00fd */

#define GBIMR1_BIM(x)	VUC_FIELD(x, 0, 4)

	VUC glimr1;	/* 00fe */

#define GLIMR1_LIM(x)	VUC_FIELD(x, 0, 4)

	VUC hole100;	/* 00ff */

/*************************************************************************
 * End Global Register Map Section
 *************************************************************************
 */


/*************************************************************************
 * Framer Transmit Side
 *************************************************************************
 */
	VUC tdmwe[4];	/* 100 - 103 */

	VUC tjbe[4];	/* 104 - 107 */

	VUC tdds[4];	/* 108 - 10B */

	VUC hole200[4]; /* 10C - 10F */

	VUC thc1;	/* 110 */

#define THC1_NOFS	(1 << 7)
#define THC1_TEOML	(1 << 6)
#define THC1_THR	(1 << 5)
#define THC1_THMS	(1 << 4)
#define THC1_TFS	(1 << 3)
#define THC1_TEOM	(1 << 2)
#define THC1_TZSD	(1 << 1)
#define THC1_TCRCD	(1 << 0)

	VUC thbse;	/* 111 */

#define THBSE_TBSE(x)	VUC_FIELD(x, 0, 8)

	VUC hole201;	/* 112 */

	VUC thc2;	/* 113 */

#define THC2_TABT	(1 << 7)
#define THC2_SBOC	(1 << 6)
#define THC2_THCEN	(1 << 5)
#define THC2_THCS(x)	VUC_FIELD(x, 0, 5)

	VUC tsacr;	/* 114 */

#define TSACR_SIAF	(1 << 7)
#define TSACR_SINAF	(1 << 6)
#define TSACR_RA	(1 << 5)
#define TSACR_SA4	(1 << 4)
#define TSACR_SA5	(1 << 3)
#define TSACR_SA6	(1 << 2)
#define TSACR_SA7	(1 << 1)
#define TSACR_SA8	(1 << 0)

	VUC hole202[3];	/* 115 - 117 */

	VUC ssie[4];	/* 118 - 11B */

	VUC hole205[4];	/* 11C - 11F */

	VUC tidr[32];	/* 120 - 13F */

#define TIDR_C(x)	VUC_FIELD(x, 0, 8)

	VUC ts[16];	/* 140 - 14F */

	VUC tcice[4];	/* 150 - 153 */

	VUC hole206[14];/* 154 - 161 */

	VUC tfdl;	/* 162 */

#define TFDL_TFDL(x)	VUC_FIELD(x, 0, 8)

	VUC tboc;	/* 163 */

#define TBOC_TBOC(x)	VUC_FIELD(x, 0, 6)

	VUC tslc1_taf;	/* 164 */

#define TSLC1_C(x)	VUC_FIELD(x, 0, 8)

#define TAF_SI		(1 << 7)

	VUC tslc2_tnaf;	/* 165 */

#define TSLC2_M(x)	VUC_FIELD(x, 6, 2)
#define TSLC2_S(x)	VUC_FIELD(x, 3, 3)
#define TSLC2_C(x)	VUC_FIELD(x, 0, 3)

#define TNAF_SI		(1 << 7)
#define TNAF_A		(1 << 5)
#define TNAF_SA4	(1 << 4)
#define TNAF_SA5	(1 << 3)
#define TNAF_SA6	(1 << 2)
#define TNAF_SA7	(1 << 1)
#define TNAF_SA8	(1 << 0)

	VUC tslc3_tsiaf;/* 166 */

#define TSLC3_S(x)	VUC_FIELD(x, 6, 2)
#define TSLC3_A(x)	VUC_FIELD(x, 6, 2)
#define TSLC3_M(x)	VUC_FIELD(x, 6, 1)

	VUC tsinaf;	/* 167 */

	VUC tra;	/* 168 */

	VUC tsa4;	/* 169 */

	VUC tsa5;	/* 16A */

	VUC tsa6;	/* 16B */

	VUC tsa7;	/* 16C */

	VUC tsa8;	/* 16D */

	VUC hole207[18];/* 16E - 17F */

	VUC tmmr;	/* 180 */

#define TMMR_FRM_EN 	(1 << 7)
#define TMMR_INIT_DONE	(1 << 6)
#define TMMR_SFTRST	(1 << 1)
#define TMMR_T1E1	(1 << 0)

	VUC tcr1;	/* 181 */

#define TCR1_TJC	(1 << 7)
#define TCR1_TFPT	(1 << 6)
#define TCR1_TCPT	(1 << 5)
#define TCR1_TSSE	(1 << 4)
#define TCR1_GB7S	(1 << 3)
#define TCR1_TB8ZS	(1 << 2)
#define TCR1_TAIS	(1 << 1)
#define TCR1_TRAI	(1 << 0)

#define TCR1_TTPT	(1 << 7)
#define TCR1_T16S	(1 << 6)
#define TCR1_TG802	(1 << 5)
#define TCR1_TSIS	(1 << 4)
#define TCR1_TSA1	(1 << 3)
#define TCR1_THDB3	(1 << 2)
#define TCR1_TAIS	(1 << 1)
#define TCR1_TCRC4	(1 << 0)

	VUC tcr2;	/* 182 */

#define TCR2_TFDLS	(1 << 7)
#define TCR2_TSLC96	(1 << 6)
#define TCR2_TDDSEN	(1 << 5)
#define TCR2_FBCT(x)	VUC_FIELD(x, 3, 2)
#define TCR2_TD4RM	(1 << 2)
#define TCR2_PDE	(1 << 1)
#define TCR2_TB7ZS	(1 << 0)

#define TCR2_AEBE	(1 << 7)
#define TCR2_AAIS	(1 << 6)
#define TCR2_ARA	(1 << 5)

	VUC tcr3;	/* 183 */

#define TCR3_TCSS(x)	VUC_FIELD(x, 4, 2)
#define TCR3_MFRS	(1 << 3)
#define TCR3_TFM	(1 << 2)
#define TCR3_IBVD	(1 << 1)
#define TCR3_TLOOP	(1 << 0)

#define TCR3_CRC4R	(1 << 0)

	VUC tiocr;	/* 184 */

#define TIOCR_TCLKINV	(1 << 7)
#define TIOCR_TSYNCINV	(1 << 6)
#define TIOCR_TSSYNCINV	(1 << 5)
#define TIOCR_TSCLKM	(1 << 4)
#define TIOCR_TSSM	(1 << 3)
#define TIOCR_TSIO	(1 << 2)
#define TIOCR_TSDW	(1 << 1)
#define TIOCR_TSM	(1 << 0)

	VUC tescr;	/* 185 */

#define TESCR_TDATFMT	(1 << 7)
#define TESCR_TGCLKEN	(1 << 6)
#define TESCR_TSZS	(1 << 4)
#define TESCR_TESALGN	(1 << 3)
#define TESCR_TESR	(1 << 2)
#define TESCR_TESMDM	(1 << 1)
#define TESCR_TESE	(1 << 0)

	VUC tcr4;	/* 186 */

#define TCR4_UALAW	(1 << 7)
#define TCR4_BINV(x)	VUC_FIELD(x, 5, 2)
#define TCR4_TJBEN	(1 << 4)
#define TCR4_TRAIM	(1 << 3)
#define TCR4_TAISM	(1 << 2)
#define TCR4_TC(x)	VUC_FIELD(x, 0, 2)

	VUC thfc;	/* 187 */

#define THFC_TFLWM(x)	VUC_FIELD(x, 0, 2)

	VUC tiboc;	/* 188 */

#define TIBOC_IBOSEL	(1 << 4)
#define TIBOC_IBOEN	(1 << 3)

	VUC tds0sel;	/* 189 */

#define TDS0SEL_TCM(x)	VUC_FIELD(x, 0, 5)

	VUC txpc;	/* 18A */

#define TXPC_THMS	(1 << 7)
#define TXPC_THEN	(1 << 6)
#define TXPC_TBPDIR	(1 << 2)
#define TXPC_TBPFUS	(1 << 1)
#define TXPC_TBPEN	(1 << 0)

	VUC tbpbs;	/* 18B */

#define TBBS_BPBSE(x)	VUC_FIELD(x, 0, 8)

	VUC hole208;	/* 18C */

	VUC thbs;	/* 18D */

#define THBS_THBSE(x)	VUC_FIELD(x, 0, 8)

	VUC tsyncc;	/* 18E */

#define TSYNCC_TSEN	(1 << 2)
#define TSYNCC_SYNCE	(1 << 1)
#define TSYNCC_RESYNC	(1 << 0)

#define TSYNCC_CRC4	(1 << 3)

	VUC hole310;	/* 18F */

	VUC tls1;	/* 190 */

#define TLS1_TESF	(1 << 7)
#define TLS1_TESEM	(1 << 6)
#define TLS1_TSLIP	(1 << 5)
#define TLS1_TSLC96	(1 << 4)
#define TLS1_TPDV	(1 << 3)
#define TLS1_TMF	(1 << 2)
#define TLS1_LOTCC	(1 << 1)
#define TLS1_LOTC	(1 << 0)

#define TLS1_TAF	(1 << 3)

	VUC tls2;	/* 191 */

#define TLS2_TFDLE	(1 << 4)
#define TLS2_TUDR	(1 << 3)
#define TLS2_TMEND	(1 << 2)
#define TLS2_TLWMS	(1 << 1)
#define TLS2_TNFS	(1 << 0)

	VUC tls3;	/* 192 */

#define TLS3_LOF	(1 << 1)
#define TLS3_LOFD	(1 << 0)

	VUC hole209[12];/* 193 - 19E */

	VUC tiir;	/* 19F */

#define TIIR_TLS1	(1 << 0)
#define TIIR_TLS2	(1 << 1)
#define TIIR_TLS3	(1 << 2)

	VUC tim1;	/* 1A0 */

#define TIM1_TESF	(1 << 7)
#define TIM1_TESEM	(1 << 6)
#define TIM1_TSLIP	(1 << 5)
#define TIM1_TSLC96	(1 << 4)
#define TIM1_TMF	(1 << 2)
#define TIM1_LOTCC	(1 << 1)
#define TIM1_LOTC	(1 << 0)

#define TIM1_TAF	(1 << 3)

	VUC tim2;	/* 1A1 */

#define TIM2_TFDLE	(1 << 4)
#define TIM2_TUDR	(1 << 3)
#define TIM2_TMEND	(1 << 2)
#define TIM2_TLWMS	(1 << 1)
#define TIM2_TNFS	(1 << 0)

	VUC tim3;	/* 1A2 */

#define TIM3_LOFD	(1 << 0)

	VUC hole210[9];	/* 1A3 - 1AB */

	VUC tcd1;	/* 1AC */

#define TCD1_C(x)	VUC_FIELD(x, 0, 8)

	VUC tcd2;	/* 1AD */

#define TCD2_C(x)	VUC_FIELD(x, 0, 8)

	VUC hole211[3];	/* 1AE - 1B0 */

	VUC trts2;	/* 1B1 */

#define TRTS_TEMPTY	(1 << 3)
#define TRTS_TFULL	(1 << 2)
#define TRTS_TLWMM	(1 << 1)
#define TRTS_TNF	(1 << 0)

	VUC hole212;	/* 1B2 */

	VUC tfba;	/* 1B3 */

#define TFBA_TFBA(x)	VUC_FIELD(x, 0, 7)

	VUC thf;	/* 1B4 */

#define THF_THD(x)	VUC_FIELD(x, 0, 8)

	VUC hole213[6];	/* 1B5 - 1BA */

	VUC tdsom;	/* 1BB */

#define TDSOM_B(x)	VUC_FIELD(x, 0, 8)

	VUC hole214[4];	/* 1BC - 1BF */

	VUC tbcs[4];	/* 1C0 - 1C3 */

	VUC tcbr[4];	/* 1C4 - 1C7 */

	VUC thscs[4];	/* 1C8 - 1CB */

	VUC tgccs[4];	/* 1CC - 1CF */

	VUC pcl[4];	/* 1D0 - 1D3 */

	VUC tbpcs[4];	/* 1D4 - 1D7*/

	VUC hole301[4];	/* 1D8 - 1DB */

	VUC thcs[4];	/* 1DC - 1DF */

	VUC hole217[32];/* 1E0 - 1FF */



/*************************************************************************
 * End Framer Transmit Side
 *************************************************************************
 */



/*************************************************************************
 * End Per Port Section
 *************************************************************************
 */

} te1[8];


/*************************************************************************
 * LIU Section
 *************************************************************************
 */
	struct _liu {

	VUC ltrcr;	/* 1000 */

#define LTRCR_RHPM	(1 << 6)
#define LTRCR_JADS(x)	VUC_FIELD(x, 4, 2)
#define LTRCR_JAPS(x)	VUC_FIELD(x, 2, 2)
#define LTRCR_T1J1E1	(1 << 1)
#define LTRCR_LSC	(1 << 0)

	VUC ltipsr;	/* 1001 */

#define LTIPSR_TG703	(1 << 7)
#define LTIPSR_TIMPTON	(1 << 6)
#define LTIPSR_TIMPL(x)	VUC_FIELD(x, 4, 2)
#define LTIPSR_L(x)	VUC_FIELD(x, 0, 3)

	VUC lmcr;	/* 1002 */

#define LMCR_TAIS	(1 << 7)
#define LMCR_ATAIS	(1 << 6)
#define LMCR_LB(x)	VUC_FIELD(x, 3, 3)
#define LMCR_TPDE	(1 << 2)
#define LMCR_RPDE	(1 << 1)
#define LMCR_TE		(1 << 0)

	VUC lrsr;	/* 1003 */

#define LRSR_OEQ	(1 << 5)
#define LRSR_UEQ	(1 << 4)
#define LRSR_RSCS	(1 << 3)
#define LRSR_TSCS	(1 << 2)
#define LRSR_OCS	(1 << 1)
#define LRSR_LOSS	(1 << 0)

	VUC lsimr;	/* 1004 */

#define LSIMR_JALTCIM	(1 << 7)
#define LSIMR_OCCIM	(1 << 6)
#define LSIMR_SCCIM	(1 << 5)
#define LSIMR_LOSCIM	(1 << 4)
#define LSIMR_JALTSIM	(1 << 3)
#define LSIMR_OCDIM	(1 << 2)
#define LSIMR_SCDIM	(1 << 1)
#define LSIMR_LOSDIM	(1 << 0)

	VUC llsr;	/* 1005 */

#define LLSR_JALTC	(1 << 7)
#define LLSR_OCC	(1 << 6)
#define LLSR_SCC	(1 << 5)
#define LLSR_LOSC	(1 << 4)
#define LLSR_JALTS	(1 << 3)
#define LLSR_OCD	(1 << 2)
#define LLSR_SCD	(1 << 1)
#define LLSR_LOSD	(1 << 0)

	VUC lrsl;	/* 1006 */

#define LRSL_RSL(x)	VUC_FIELD(x, 4, 4)

	VUC lrismr;	/* 1007 */

#define LRISMR_RIMPON	(1 << 6)
#define LRISMR_RIMPM(x)	VUC_FIELD(x, 0, 3)

	VUC lrcr;	/* 1008 */

#define LRCR_RG703	(1 << 7)
#define LRCR_RTR	(1 << 3)
#define LRCR_RMONEN	(1 << 2)
#define LRCR_RSMS(x)	VUC_FIELD(x, 0, 2)

	VUC hole[0x1020-0x1009];	/* 1009 - 101f */
	} liu[8];

/*************************************************************************
 * End LIU Section
 *************************************************************************
 */


/*************************************************************************
 * Bert Section
 *************************************************************************
 */
	struct _bert {

	VUC bawc;	/* 1100 */

#define BAWC_ACNT(x)	VUC_FIELD(x, 0, 8)

	VUC brp[4];	/* 1101 - 1104 */

	VUC bc1;	/* 1105 */

#define BC1_TC		(1 << 7)
#define BC1_TINV	(1 << 6)
#define BC1_RINV	(1 << 5)
#define BC1_PS(x)	VUC_FIELD(x, 2, 3)
#define BC1_LC		(1 << 1)
#define BC1_RESYNC	(1 << 0)

	VUC bc2;	/* 1106 */

#define BC2_EIB(x)	VUC_FIELD(x, 5, 3)
#define BC2_SBE	(1 << 4)
#define BC2_RPL(x)	VUC_FIELD(x, 0, 4)

	VUC bbc[4];	/* 1107 - 110A */

	VUC bec[3];	/* 110B - 110D */

	VUC bsr;	/* 110E */

#define BSR_BBED	(1 << 6)
#define BSR_RBRA01	(1 << 5)
#define BSR_RSYNC	(1 << 4)
#define BSR_BRA(x)	VUC_FIELD(x, 2, 2)
#define BSR_BRLOS	(1 << 1)
#define BSR_BSYNC	(1 << 0)

	VUC bsim;	/* 110F */

#define BSIM_BBED	(1 << 6)
#define BSIM_BBCO	(1 << 5)
#define BSIM_BEC0	(1 << 4)
#define BSIM_BRA(x)	VUC_FIELD(x, 2, 2)
#define BSIM_BRLOS	(1 << 1)
#define BSIM_BSYNC	(1 << 0)

	} bert[8];

	VUC endbert[0x1400-0x1180];

	struct _extbert {

	VUC bc3;	/* 1400 */

#define BC3_55OCT	(1 << 1)
#define BC3_BALIGN	(1 << 0)

	VUC brsr;	/* 1401 */

#define BRSR_BRA(x)	VUC_FIELD(x, 2, 2)
#define BRSR_BRLOS	(1 << 1)
#define BRSR_BSYNC	(1 << 0)

	VUC blsr1;	/* 1402 */

#define BLSR1_BRA1C	(1 << 7)
#define BLSR1_BRA0C	(1 << 6)
#define BLSR1_BRLOSC	(1 << 5)
#define BLSR1_BSYNCC	(1 << 4)
#define BLSR1_BRA1D	(1 << 3)
#define BLSR1_BRA0D	(1 << 2)
#define BLSR1_BRLOSD	(1 << 1)
#define BLSR1_BSYNCD	(1 << 0)

	VUC bsim1;	/* 1403 */

#define BSIM1_BRA1C	(1 << 7)
#define BSIM1_BRA0C	(1 << 6)
#define BSIM1_BRLOSC	(1 << 5)
#define BSIM1_BSYNCC	(1 << 4)
#define BSIM1_BRA1D	(1 << 3)
#define BSIM1_BRA0D	(1 << 2)
#define BSIM1_BRLOSD	(1 << 1)
#define BSIM1_BSYNCD	(1 << 0)

	VUC blsr2;	/* 1404 */

#define BLSR2_BED	(1 << 2)
#define BLSR2_BBCO	(1 << 1)
#define BLSR2_BECO	(1 << 0)

	VUC bsim2;	/* 1405 */

#define BSIM2_BED	(1 << 2)
#define BSIM2_BBCO	(1 << 1)
#define BSIM2_BECO	(1 << 0)

	VUC holeext[0x1410-0x1406];

	} extbert[8];

/*************************************************************************
 * End Bert Section
 *************************************************************************
 */
	VUC endext[0x1500-0x1480];

/*************************************************************************
 * HDLC Section
 *************************************************************************
 */
	struct _hdlc {
	VUC th256cr1;		/* 1500 */

#define TH256CR1_TPSD		(1 << 6)
#define TH256CR1_TFEI		(1 << 5)
#define TH256CR1_TIFV		(1 << 4)
#define TH256CR1_TBRE		(1 << 3)
#define TH256CR1_TDIE		(1 << 2)
#define TH256CR1_TFPD		(1 << 1)
#define TH256CR1_TFRST		(1 << 0)

	VUC th256cr2;		/* 1501 */

#define TH256CR2_TDAL(x)	VUC_FIELD(x, 0, 5)

	VUC th256fdr1;		/* 1502 */

#define TH256FDR1_TDPE		(1 << 0)

	VUC th256fdr2;		/* 1503 */

#define TH256FDR2_TFD(x)	VUC_FIELD(x, 0, 8)

	VUC th256sr1;		/* 1504 */

#define TH256SR1_TFF		(1 << 2)
#define TH256SR1_TFE		(1 << 1)
#define TH256SR1_THDA		(1 << 0)

	VUC th256sr2;		/* 1505 */

#define TH256SR2_TFFL(x)	VUC_FIELD(x, 0, 6)

	VUC th256srl;		/* 1506 */

#define TH256SRL_TFOL		(1 << 5)
#define TH256SRL_TFUL		(1 << 4)
#define TH256SRL_TPEL		(1 << 3)
#define TH256SRL_TFEL		(1 << 1)
#define TH256SRL_THDAL		(1 << 0)

	VUC hole1507;		/* 1507 */

	VUC th256srie;		/* 1508 */

#define TH256SRIE_TFOIE		(1 << 5)
#define TH256SRIE_TFUIE		(1 << 4)
#define TH256SRIE_TPEIE		(1 << 3)
#define TH256SRIE_TFEIE		(1 << 1)
#define TH256SRIE_THDAIE	(1 << 0)

	VUC hole1509[0x1510-0x1509];	/* 1509-150f */

	VUC rh256cr1;		/* 1510 */

#define RH256CR1_RBRE		(1 << 3)
#define RH256CR1_RDIE		(1 << 2)
#define RH256CR1_RFPD		(1 << 1)
#define RH256CR1_RFRST		(1 << 0)

	VUC rh256cr2;		/* 1511 */

#define RH256CR2_RDAL(x)	VUC_FIELD(x, 0, 5)

	VUC hole1512[2];	/* 1512-1513 */

	VUC fh256sr1;		/* 1514 */

#define RH256SR1_RFF		(1 << 2)
#define RH256SR1_RFE		(1 << 1)
#define RH256SR1_RHDA		(1 << 0)

	VUC hole1515;		/* 1515 */

	VUC rh256srl;		/* 1516 */

#define RH256SRL_RFOL		(1 << 7)
#define RH256SRL_RPEL		(1 << 4)
#define RH256SRL_RPSL		(1 << 3)
#define RH256SRL_RFFL		(1 << 2)
#define RH256SRL_RHDAL		(1 << 0)

	VUC hole1517;		/* 1517 */

	VUC rh256srie;		/* 1518 */

#define RH256SRIE_RFOIE		(1 << 7)
#define RH256SRIE_RFEIE		(1 << 4)
#define RH256SRIE_RPSIE		(1 << 3)
#define RH256SRIE_RFFIE		(1 << 2)
#define TH256SRIE_RHDAIE	(1 << 0)

	VUC hole1519[3];	/* 1519-151B */

	VUC rh256fdr1;		/* 151C */

#define RH256FDR1_RPS(x)	VUC_FIELD(x, 1, 3)
#define RH256FDR1_RFDV		(1 << 0)

	VUC rh256fdr2;		/* 151D */

#define RH256FDR2_RFD(x)	VUC_FIELD(x, 0, 8)

	VUC hole151E[2];	/* 151E-151F */

	} hdlc[8];
/*************************************************************************
 * End HDLC Section
 *************************************************************************
 */

	VUC endhole[0x2000-0x1600];

} DEVICE;

typedef struct _te1span FRAMER;
typedef struct _liu LIU;
typedef struct _bert BERT;
typedef struct _extbert EXTBERT;
typedef struct _hdlc HDLC;


enum BACKPLANE_REFERENCE
{
	LIU_RCLK1 = 0,
	LIU_RCLK2 = 1,
	LIU_RCLK3 = 2,
	LIU_RCLK4 = 3,
	LIU_RCLK5 = 4,
	LIU_RCLK6 = 5,
	LIU_RCLK7 = 6,
	LIU_RCLK8 = 7,
	MCLK = 8,
	REFCLKIO = 9
};

enum TCLK_REFERNCE
{
	TCLK_PIN = 0,
	TCLK_REFCLKIO = 1
};

enum SIG_TYPE
{
	CCS_TYPE,  /* ss7 or pri */
	CAS_TYPE   /* r2 or china no.1 */
};

enum SLOT_ACTIVE
{
	VOICE_ACTIVE,
	VOICE_INACTIVE
};

enum LOOPBACK_TYPE
{
	NO_LP = 0,
	REMOTE_JA_LP = 1,
	ANALOG_LP = 2,
	REMOTE_NO_JA_LP = 3,
	LOCAL_LP = 4,
	DUAL_LP = 5,
	FRAME_LOCAL_LP = 6,
	FRAME_REMOTE_LP = 7
};

extern void set_ds26518_interrupt(int e1_no, int enable);
extern void set_ds26518_global_interrupt(int enable);
extern void ds26518_e1_slot_enable(int e1_no, int slot, enum SLOT_ACTIVE active);
extern void set_ds26518_master_clock(enum BACKPLANE_REFERENCE back_ref);
extern void set_ds26518_slave_clock(void);
extern void set_ds26518_signal_slot(int e1_no, UC signal_slot);
extern UC read_ds26518_signal_slot(int e1_no);
extern void set_ds26518_crc4(int e1_no, int enable);

extern void ds26518_global_init(void);
extern void ds26518_port_init(int e1_no, enum SIG_TYPE sig_type);

extern void reset_hdlc64_receive(int e1_no);
extern void reset_hdlc64_transmit(int e1_no);

extern void disable_e1_transmit(int e1_no);
extern void enable_e1_transmit(int e1_no);
extern void set_ds26518_loopback(int e1_no, enum LOOPBACK_TYPE lp_type);
extern void ds26518_isr(void);

#endif /* Build for Specific Driver */
