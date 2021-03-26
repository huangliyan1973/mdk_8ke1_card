#ifndef INC_CSU_IF_H_
#define INC_CSU_IF_H_

#include "lwip/ip_addr.h"
#include "lwip/arch.h"

#define  MSG_DEBUG

#define  TO_OMC				2
#define  SS7_UDP_PORT		4950
#define	 ISDN_UDP_PORT		4951
#define  SNMP_UDP_PORT		4957
#define  OTHER_UDP_PORT		4952

#define  OTHER_SIO			0x87
#define  MTP2_COMMAND_SIO   0xFF
#define  ISDN_SIO			0x08
#define  ISDN_MNG			0xFE

#define  MAPB_HEAD_SIZE  (sizeof(struct mapb_head))
#define  OTHER_MSG_CONTENT_SIZE		5

struct mapb_head {
	u8_t		msu_len;
	u8_t		reserved;
	u8_t		sio;
	u8_t		dst_ref[3];
	u8_t		org_ref[3];
	u8_t		msg_type;
}__attribute__ ((packed));

/* Receive from msc port: 4950 */
struct other_msg {
	struct mapb_head	m_head;
	u8_t				src_slot;
	u8_t				dst_id;
	u8_t				dst_slot;
	u8_t				tone_no;
	u8_t				playtimes;
}__attribute__ ((packed));

/* Send to msc port :4952 */
struct msc_msg {
	struct mapb_head	m_head;
	u8_t				src_slot;
	u8_t				dst_id;
	u8_t				dst_slot;
	u8_t				decode_type;
	u8_t				digit;
	u8_t				other[32];
}__attribute__ ((packed));

/* Send/Receive to/from msc  port : 4950 */ 
struct ss7_msg {
	u8_t				e1_no;
	u8_t				msg_len;
	u8_t				contents[1];
}__attribute__ ((packed));

struct server_msg {
	u8_t				e1_no;
	u8_t				msg_len;
	u8_t 				sio;
	union{
		struct{   
			u8_t    server_ip;                   /* SN_IP - 32, = 66/67 */
			u8_t    command;                  /* Primitive Type */
			u8_t    dest_ip[8];
		}__attribute__ ((packed)) serv_comm;

		struct{
			u8_t    cr_len;                 /* Call Reference Length */
			u8_t    callref[2];             /* Call Reference */
			u8_t    msgtype;                /* Message Type */
			u8_t    contents[1];         	/* L3 Message Content */
		}__attribute__ ((packed)) isdn;

		struct {
			u8_t 	contents[1];
		} __attribute__((packed)) ss7;

	}__attribute__((packed)) msg;

} __attribute__((packed));

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
	u8_t    e1_install;
	u8_t    e1_l2_install_fg;
	u8_t    e1_l1_alarm;
	u8_t 	e1_l2_alarm;
	u8_t 	echo_fg;
	u8_t 	cpu_loading;
	u8_t 	src_clk;
	u8_t 	freq;
	u8_t 	master_clk_fg;
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
	u8_t    info[8];
}__attribute__ ((packed)) heart_t;

/* whole heartbeat msg len = 58 */
typedef struct {
	u8_t            	sys_id;     				// 0
	u8_t            	subsys_id;  				// 1
	u32_t    			timestamp;   				// 2
	u8_t				omcled[8];					// 6
	u8_t				cp_id;						// 14
	u8_t				alarm_code;					// 15
	u8_t 				alarm_level;				// 16
	u8_t 				hbextLen;					// 17
	u8_t 				hb_version[3];				// 18
	u8_t				reserved;					// 21
    component_t   		component; 					// 22
	u8_t				hb_lock_state;				// 31
	u8_t				crc4_count[8];				// 32
	u8_t				self_check_result;			// 40
	u8_t				hb_conf;					// 41
	u8_t				hb_fdl[16];					// 42
}__attribute__ ((packed)) card_heart_t;

typedef struct {
	u8_t    sys_id;					//0
	u8_t    subsys_id;				//1
	u32_t   timestamp;				//2
	u8_t    led_ctl_code[8];		//6
	u8_t    alarm_component;		//14
	u8_t    alarm_code;				//15
	u8_t    reserved;				//16
	u8_t    l1_status;				//17
	u8_t    l2_status[8];			//18
	u8_t 	dst_ip[8];				//26
	u8_t 	retrieved_bsnt[8];		//34
	u8_t 	mtp2_soft_version;		//42
	u8_t 	e1_port_type;			//43
	u8_t 	is_NT;					//44
	u8_t 	sysnc_clock[16];		//45
	u8_t    mtp2_mode;				//61
} __attribute__((packed)) mtp2_heart_t;

extern mtp2_heart_t mtp2_heart_msg;

/* STM32F407 PB7 pin control the master clock input */
#define MASTER_CLK		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET)
#define SLAVE_CLK		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET)

extern ip4_addr_t  sn0;
extern ip4_addr_t  sn1;
extern ip4_addr_t  omc;

#define plat_no		((card_id >> 4) & 1)

extern void send_ss7_msg(u8_t link_no, u8_t *buf, u8_t len);

extern void send_other_msg(struct other_msg *msg, u8_t len);

extern void send_isdn_msg(u8_t link_no, u8_t *buf, u8_t len);

extern void send_trap_msg(u8_t dst_flag);

extern void server_interface_init(void);

extern void update_no1_e1(u8_t new_value);

extern void period_10s_proc(void *arg);

extern void start_period_proc(void);

extern void snmp_8ke1_init(void);

extern void hb_msg_init(void);
extern void mtp2_heart_msg_init(void);

extern void link_in_service(int e1_no);

extern void link_outof_service(int e1_no, u8_t alarm_code);

extern void send_mtp2_trap_msg(void);

extern void set_mtp2_heart_msg_dstip(u8_t *dstip);

extern void send_card_heartbeat(u8_t dst_flag);

extern void update_e1_enable(u8_t new_value);

extern void set_card_e1_led(void);

extern void init_r2_param(void);

extern void init_mfc_slot(void);

extern void period_50ms_proc(void *arg);

extern void period_20ms_proc(void *arg);

extern void period_500ms_proc(void *arg);

extern void period_10s_proc(void *arg);

#endif /* INC_CSU_IF_H_ */
