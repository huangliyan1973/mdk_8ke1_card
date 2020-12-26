#ifndef INC_CSU_IF_H_
#define INC_CSU_IF_H_

#include "lwip/ip_addr.h"
#include "lwip/arch.h"

#define  SS7_UDP_PORT		4950
#define	 ISDN_UDP_PORT		4951
#define  SNMP_UDP_PORT		4957
#define  OTHER_UDP_PORT		4952

typedef struct {
	u8_t		msgCreatedTime[15];
	u32_t	    msgSrcIp;
	u16_t	    msgSrcPort;
	u32_t	    msgDstIp;
	u16_t 	    msgDstPort;
	u32_t	    msgBroadcast;
	u16_t	    msgLens;

	u8_t		*msgContents;
}__attribute__ ((packed)) udpMsg_t;

#define MTP2_ACTIVE_LINK	1
#define MTP2_DEACTIVE_LINK	2
#define MTP2_STOP_L2		3
#define MTP2_EMERGEN_ALIGNMENT	4

#define SIGNAL_TOP			7
#define ISDN_PD				8

typedef struct {
	u8_t linkNo;
	u8_t msgLens;

	union{
		struct {
			u8_t		pd;   		/* Protocol Discriminator */
			u8_t		crLens;		/* Call Reference Length */
			u8_t		callRef[2]; /* Call Reference */
			u8_t		msgType;
			u8_t		*msgContents;
		}__attribute__ ((packed)) isdnMsg;

		struct {
			u8_t 	    sio;		/* 0,1 = test  3 = isup, 4 = tup, 5 = sccp */
			u8_t		*msgContents;
		}__attribute__ ((packed)) ss7Msg;

		struct {
			u8_t		flag;      /* = 0xff */
			u8_t		platId;
			u8_t		mtp2Command;
		}__attribute__ ((packed)) mtp2Msg;
	}__attribute__ ((packed)) msg;

}__attribute__ ((packed)) signalMsg_t;


#define CON_TIME_SLOT       0
#define CON_TONE            2
#define CON_DTMF            3
#define CON_CONN_GRP        4
#define CON_DISC_GRP        5
#define CON_DEC_DTMF        7
#define CON_DEC_MFC     	9

typedef struct {
	u8_t		msgLens;
	u8_t		reserved;
	u8_t		sio;
	u8_t		dstRef[3];
	u8_t		srcRef[3];
	u8_t		msgType;

	u8_t		srcSlot;
	u8_t		dstId;
	u8_t		dstSlot;
	u8_t		commandType;
	u8_t		digit;
	u8_t		*other;

}__attribute__ ((packed)) commandMsg_t;


/*heartbeat struct */
#define COMPONENT_NUM	9
#define LED_NUM			16

typedef struct {
	u8_t    flag;
	u8_t    alarm_code;
	u8_t    status_len;
	u8_t    status[256];
}__attribute__ ((packed)) component_t;

typedef struct{
	u8_t    sys_id;
	u8_t    subsys_id;
	u32_t    timestamp;
	u8_t    led_color[8];
	u8_t    component_id;
	u8_t    alarm_code;
	u8_t    reserved;
	u8_t    length;
	u8_t    *info;
}__attribute__ ((packed)) heart_t;

typedef struct {
	u8_t            	sys_id;
	u8_t            	subsys_id;
	u8_t            	led[LED_NUM];
    component_t   		component[COMPONENT_NUM]; //COMPONENT_NUM=9
    u8_t            	alarm_num;
    heart_t   			msg;
}__attribute__ ((packed)) heartMsg_t;

extern ip4_addr_t  sn0;
extern ip4_addr_t  sn1;
extern ip4_addr_t  omc;

#define plat_no		((card_id >> 4) & 1)

#endif /* INC_CSU_IF_H_ */
