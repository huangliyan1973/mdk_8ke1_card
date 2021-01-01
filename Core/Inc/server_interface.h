#ifndef INC_CSU_IF_H_
#define INC_CSU_IF_H_

#include "lwip/ip_addr.h"
#include "lwip/arch.h"

#define  MSG_DEBUG

#define  SS7_UDP_PORT		4950
#define	 ISDN_UDP_PORT		4951
#define  SNMP_UDP_PORT		4957
#define  OTHER_UDP_PORT		4952

#define  OTHER_SIO			0x87
#define  MTP2_COMMAND_SIO   0xFF

struct ss7_head {
	u8_t		e1_no;
	u8_t		len;
	u8_t		sio;
	u8_t		msg[1];
}__attribute__ ((packed));

struct mapb_head {
	u8_t		msu_len;
	u8_t		reserved;
	u8_t		sio;
	u8_t		dst_ref[3];
	u8_t		org_ref[3];
	u8_t		msg_type;
}__attribute__ ((packed));

struct ip_head {
	u8_t		msgCreatedTime[15];
	u32_t	    msgSrcIp;
	u16_t	    msgSrcPort;
	u32_t	    msgDstIp;
	u16_t 	    msgDstPort;
	u32_t	    msgBroadcast;
	u16_t	    msgLens;
}__attribute__ ((packed));

/* Receive from msc port: 4950 */
struct other_msg {
	struct ip_head		ip_head;
	struct mapb_head	m_head;
	u8_t				src_slot;
	u8_t				dst_id;
	u8_t				dst_slot;
	u8_t				tone_no;
	u8_t				playtimes;
}__attribute__ ((packed));

/* Send to msc port :4952 */
struct msc_msg {
	struct ip_head		ip_head;
	struct mapb_head	m_head;
	u8_t				src_slot;
	u8_t				dst_id;
	u8_t				dst_slot;
	u8_t				decode_type;
	u8_t				digit;
	u8_t				other[1];
}__attribute__ ((packed));

/* Send/Receive to/from msc  port : 4950 */ 
struct ss7_msg {
	struct ip_head 		ip_head;
	struct ss7_head 	msg;
}__attribute__ ((packed));

/* Send to msc port : 4951 */ 
struct isdn_msg {
	struct ip_head		ip_head;
	u8_t				e1_no;
	u8_t				msg_len;
	union{
		struct{   
			u8_t    pd;                     /* Protocol Discriminator */
			u8_t    e1ip;                   /* E1_IP - 32 */
			u8_t    primi;                  /* Primitive Type */
		}__attribute__ ((packed)) primimsg;

		struct{
			u8_t    pd;                     /* Protocol Discriminator */
			u8_t    cr_len;                 /* Call Reference Length */
			u8_t    callref[2];             /* Call Reference */
			u8_t    msgtype;                /* Message Type */
			u8_t    l3content[1];         /* L3 Message Content */
		}__attribute__ ((packed)) l3msg;
	}__attribute__ ((packed)) msg;

}__attribute__ ((packed));



#define MTP2_ACTIVE_LINK	1
#define MTP2_DEACTIVE_LINK	2
#define MTP2_STOP_L2		3
#define MTP2_EMERGEN_ALIGNMENT	4

#define SIGNAL_TOP			7
#define ISDN_PD				8


#define CON_TIME_SLOT       0
#define CON_TONE            2
#define CON_DTMF            3
#define CON_CONN_GRP        4
#define CON_DISC_GRP        5
#define CON_DEC_DTMF        7
#define CON_DEC_MFC     	9


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

extern void send_ss7_msg(u8_t link_no, u8_t *buf, u8_t len);

extern void send_other_msg(struct other_msg *msg, u8_t len);

extern void send_isdn_msg(u8_t link_no, u8_t *buf, u8_t len);

extern void server_interface_init(void);

#endif /* INC_CSU_IF_H_ */
