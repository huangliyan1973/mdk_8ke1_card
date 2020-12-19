#ifndef INC_CSU_IF_H_
#define INC_CSU_IF_H_

#define  SS7_UDP_PORT		4950
#define	 ISDN_UDP_PORT		4951
#define  SNMP_UDP_PORT		4957
#define  OTHER_UDP_PORT		4952

typedef struct {
	uint8_t		msgCreatedTime[15];
	uint32_t	msgSrcIp;
	uint16_t	msgSrcPort;
	uint32_t	msgDstIp;
	uint16_t 	msgDstPort;
	uint32_t	msgBroadcast;
	uint16_t	msgLens;

	uint8_t		*msgContents;
}udpMsg_t;

#define MTP2_ACTIVE_LINK	1
#define MTP2_DEACTIVE_LINK	2
#define MTP2_STOP_L2		3
#define MTP2_EMERGEN_ALIGNMENT	4

#define SIGNAL_TOP			7
#define ISDN_PD				8

typedef struct {
	uint8_t linkNo;
	uint8_t msgLens;

	union{
		struct {
			uint8_t		pd;   		/* Protocol Discriminator */
			uint8_t		crLens;		/* Call Reference Length */
			uint8_t		callRef[2]; /* Call Reference */
			uint8_t		msgType;
			uint8_t		*msgContents;
		}isdnMsg;

		struct {
			uint8_t 	sio;		/* 0,1 = test  3 = isup, 4 = tup, 5 = sccp */
			uint8_t		*msgContents;
		}ss7Msg;

		struct {
			uint8_t		flag;      /* = 0xff */
			uint8_t		platId;
			uint8_t		mtp2Command;
		}mtp2Msg;
	}msg;

}signalMsg_t;


#define CON_TIME_SLOT       0
#define CON_TONE            2
#define CON_DTMF            3
#define CON_CONN_GRP        4
#define CON_DISC_GRP        5
#define CON_DEC_DTMF        7
#define CON_DEC_MFC     	9

typedef struct {
	uint8_t		msgLens;
	uint8_t		reserved;
	uint8_t		sio;
	uint8_t		dstRef[3];
	uint8_t		srcRef[3];
	uint8_t		msgType;

	uint8_t		srcSlot;
	uint8_t		dstId;
	uint8_t		dstSlot;
	uint8_t		commandType;
	uint8_t		digit;
	uint8_t		*other;

}commandMsg_t;


/*heartbeat struct */
#define COMPONENT_NUM	9
#define LED_NUM			16

typedef struct {
	uint8_t    flag;
	uint8_t    alarm_code;
	uint8_t    status_len;
	uint8_t    status[256];
}component_t;

typedef struct{
	uint8_t    sys_id;
	uint8_t    subsys_id;
	uint8_t    timestamp[4];
	uint8_t    led_color[8];
	uint8_t    component_id;
	uint8_t    alarm_code;
	uint8_t    reserved;
	uint8_t    length;
	uint8_t    *info;
}heart_t;

typedef struct {
	uint8_t            	sys_id;
	uint8_t            	subsys_id;
	uint8_t            	led[LED_NUM];
    component_t   		component[COMPONENT_NUM]; //COMPONENT_NUM=9
    uint8_t            	alarm_num;
    heart_t   			msg;
}heartMsg_t;

#endif /* INC_CSU_IF_H_ */
