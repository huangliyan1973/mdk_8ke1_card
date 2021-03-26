#ifndef MTP_H
#define MTP_H

#include <stdint.h>

#define E1_LINKS_MAX			8
#define MTP_MBOX_SIZE 			6

#define MTP2_ACTIVE_LINK    	1
#define MTP2_DEACTIVE_LINK  	2
#define MTP2_STOP_L2        	3	
#define MTP2_EMERGEN_ALIGNMENT  4


#define SS7_PROTO_ISUP          5
#define SS7_PROTO_TUP           4
#define SS7_PROTO_SCCP          3
#define ISDN_PRI	            8

#define PRI_NETWORK		        1
#define PRI_CPE			        2

#define SS7_PROTO_TYPE			1
#define PRI_PROTO_TYPE			2
#define NO1_PROTO_TYPE			3

#define SS7_PORT_ENABLE(index)      (((e1_params.e1_port_type[card_id & 0x0F] >> index) & 1) == 0)
#define CHN_NO1_PORT_ENABLE(index)  (((e1_params.no1_enable[card_id & 0x0F] >> index) & 1) == 1)
#define E1_PORT_ENABLE(index)       (((e1_params.e1_enable[card_id & 0x0F] >> index) & 1) == 1)
#define PRI_NETWORK_ENABLE(index)   (((e1_params.isdn_port_type[card_id & 0x0F] >> index) & 1) == 1)

#define Q921_FRAMETYPE_MASK	0x3

#define Q921_FRAMETYPE_U	0x3
#define Q921_FRAMETYPE_I	0x0
#define Q921_FRAMETYPE_S	0x1

#define Q921_TEI_GROUP					127
#define Q921_TEI_PRI					0
#define Q921_TEI_GR303_EOC_PATH			0
#define Q921_TEI_GR303_EOC_OPS			4
#define Q921_TEI_GR303_TMC_SWITCHING	0
#define Q921_TEI_GR303_TMC_CALLPROC		0
#define Q921_TEI_AUTO_FIRST				64
#define Q921_TEI_AUTO_LAST				126

#define Q921_SAPI_CALL_CTRL		0
#define Q921_SAPI_GR303_EOC		1
#define Q921_SAPI_GR303_TMC_SWITCHING	1
#define Q921_SAPI_GR303_TMC_CALLPROC	0


#define Q921_SAPI_PACKET_MODE		1
#define Q921_SAPI_X25_LAYER3      	16
#define Q921_SAPI_LAYER2_MANAGEMENT	63

enum q921_tei_identity {
	Q921_TEI_IDENTITY_REQUEST = 1,
	Q921_TEI_IDENTITY_ASSIGNED = 2,
	Q921_TEI_IDENTITY_DENIED = 3,
	Q921_TEI_IDENTITY_CHECK_REQUEST = 4,
	Q921_TEI_IDENTITY_CHECK_RESPONSE = 5,
	Q921_TEI_IDENTITY_REMOVE = 6,
	Q921_TEI_IDENTITY_VERIFY = 7,
};

enum Q931_DL_EVENT {
	Q931_DL_EVENT_NONE,
	Q931_DL_EVENT_DL_ESTABLISH_IND,
	Q931_DL_EVENT_DL_ESTABLISH_CONFIRM,
	Q931_DL_EVENT_DL_RELEASE_IND,
	Q931_DL_EVENT_DL_RELEASE_CONFIRM,
	Q931_DL_EVENT_TEI_REMOVAL,
};

typedef struct q921_header {
	uint8_t	ea1:1;		/* Extended Address (0) */
	uint8_t	c_r:1;		/* Command/Response (0 if CPE, 1 if network) */
	uint8_t	sapi:6;	    /* Service Access Point Indentifier (always 0 for PRI) (0) */
	uint8_t	ea2:1;		/* Extended Address Bit (1) */
	uint8_t	tei:7;		/* Terminal Endpoint Identifier (0) */

} __attribute__ ((packed)) q921_header;

/* A Supervisory Format frame */
typedef struct q921_s {
	struct q921_header h;	/* Header */
	uint8_t ft:2;			/* Frame type bits (01) */
	uint8_t ss:2;			/* Supervisory frame bits */
	uint8_t x0:4;			/* Unused */
	uint8_t p_f:1;			/* Poll/Final bit */
	uint8_t n_r:7;			/* Number Received */

	uint8_t data[1];		/* Any further data */
} __attribute__ ((packed)) q921_s;

/* An Unnumbered Format frame */
typedef struct q921_u {
	struct q921_header h;	/* Header */	
	uint8_t ft:2;			/* Frame type bits (11) */
	uint8_t m2:2;			/* Two more modifier bits */
	uint8_t p_f:1;			/* Poll/Final bit */
	uint8_t m3:3;			/* Top 3 modifier bits */
	
	uint8_t data[1];		/* Any further data */
} __attribute__ ((packed)) q921_u;

/* An Information frame */
typedef struct q921_i {
	struct q921_header h;	/* Header */
	uint8_t ft:1;			/* Frame type (0) */
	uint8_t n_s:7;			/* Number sent */
	uint8_t p_f:1;			/* Poll/Final bit */
	uint8_t n_r:7;			/* Number received */

	uint8_t data[1];		/* Any further data */
} __attribute__ ((packed)) q921_i;

typedef union {
	q921_u u;
	q921_s s;
	q921_i i;
	struct q921_header h;
	uint8_t raw[5];
} __attribute__ ((packed)) q921_h;

enum q921_tx_frame_status {
	Q921_TX_FRAME_NEVER_SENT,
	Q921_TX_FRAME_PUSHED_BACK,
	Q921_TX_FRAME_SENT,
};

typedef struct q921_frame {
	struct q921_frame *next;			/*!< Next in list */
	int len;							/*!< Length of header + body */
	enum q921_tx_frame_status status;	/*!< Tx frame status */
	q921_i h;							/*!< Actual frame contents. */
} q921_frame;

typedef enum q921_state {
	/* All states except Q921_DOWN are defined in Q.921 SDL diagrams */
	Q921_TEI_UNASSIGNED = 1,
	Q921_ASSIGN_AWAITING_TEI = 2,
	Q921_ESTABLISH_AWAITING_TEI = 3,
	Q921_TEI_ASSIGNED = 4,
	Q921_AWAITING_ESTABLISHMENT = 5,
	Q921_AWAITING_RELEASE = 6,
	Q921_MULTI_FRAME_ESTABLISHED = 7,
	Q921_TIMER_RECOVERY = 8,
} q921_state;

/*! TEI identity check procedure states. */
enum q921_tei_check_state {
	/*! Not participating in the TEI check procedure. */
	Q921_TEI_CHECK_NONE,
	/*! No reply to TEI check received. */
	Q921_TEI_CHECK_DEAD,
	/*! Reply to TEI check received in current poll. */
	Q921_TEI_CHECK_REPLY,
	/*! No reply to current TEI check poll received.  A previous poll got a reply. */
	Q921_TEI_CHECK_DEAD_REPLY,
};

#define MTP_MAX_PCK_SIZE 256
#define U_S_PCK_SIZE	32
#define U_S_PCK_BUFF_SIZE  32

typedef struct mtp2_state{
	enum {
		/* Link is stopped by management command, will not go up until
		   started explicitly. */
		MTP2_DOWN,
		/* Initial alignment has started, link is transmitting 'O', but no 'O',
		   'N', or 'E' has been received. */
		MTP2_NOT_ALIGNED,
		/* 'O' has been received, 'N' or 'E' is transmitted. */
		MTP2_ALIGNED,
		/* 'N' or 'E' is transmitted and received. Runs for the duration of T4 to
		   check that the link is of sufficient quality in terms of error rate. */
		MTP2_PROVING,
		/* Local T4 expired, and we are sending FISU, but remote is still
		   proving. */
		MTP2_READY,
		/* The link is active sending and receiving FISU and MSU. */
		MTP2_INSERVICE,
	} state;

	uint8_t send_fib;
	uint8_t send_bsn, send_bib;

	uint8_t sls;
	uint8_t subservice;

	uint8_t e1_no;

	uint8_t rx_buf[MTP_MAX_PCK_SIZE];
	uint16_t rx_len;

	uint8_t tx_buffer[MTP_MAX_PCK_SIZE];
	uint16_t tx_len;

	/* Last few raw bytes received, for debugging link errors. */
	uint8_t backbuf[32];
	uint16_t backbuf_idx;

	/*Retransmit buffer */
	struct {
		uint16_t len;
		uint8_t buf[MTP_MAX_PCK_SIZE];
	}retrans_buf[128];

	/* Retransmit counter; if this is != -1, it means that retransmission is
	   taking place, with this being the next sequence number to retransmit. */
	int retrans_seq;
	/* Last sequence number ACK'ed by peer. */
	uint8_t retrans_last_acked;
	/* Last sequence number sent to peer. */
	uint8_t retrans_last_sent;

	uint16_t bsn_errors;

	uint16_t protocal;

	uint16_t init_down;

	uint16_t emergent_setup;

	uint32_t last_send_fisu;

	uint32_t last_send_sif;

	uint8_t sccp_flag;

	uint32_t sin_scount ;
	uint32_t sin_rcount ;
	uint32_t fisu_scount ;
	uint32_t fisu_rcount ;
	uint32_t miss_fisu_count;

	/****************************ISDN PRI Part *************************/

	uint16_t pri_mode;

	enum q921_state q921_state;

	enum q921_tei_check_state	tei_check;

	uint8_t sapi;
	uint8_t tei;
	uint8_t ri;   /*TEI assignment random indicator */

	/*! V(A) - Next I-frame sequence number needing ack */
	uint8_t v_a;
	/*! V(S) - Next I-frame sequence number to send */
	uint8_t v_s;
	/*! V(R) - Next I-frame sequence number expected to receive */
	uint8_t v_r;

	struct {
		uint16_t len;
		uint8_t buf[U_S_PCK_SIZE];
	}u_s_frame[U_S_PCK_BUFF_SIZE];

	uint16_t s_h, s_t;

	/*! T-200 retransmission timer */
	uint16_t t200_timer;
	/*! Retry Count (T200) */
	uint16_t RC;
	
	uint16_t n202_counter;
	
	uint16_t t201_expirycnt;

	/* MDL variables */
	//int mdl_timer;
	uint16_t mdl_error;
	unsigned int mdl_free_me:1;

	unsigned int peer_rx_busy:1;
	unsigned int own_rx_busy:1;
	unsigned int acknowledge_pending:1;
	unsigned int reject_exception:1;
	unsigned int l3_initiated:1;

}mtp2_t;

extern void mtp_init(void);

extern void mtp_cleanup(void);

extern mtp2_t *get_mtp2_state(uint8_t link_no);

extern void mtp2_queue_msu(uint8_t e1_no, uint8_t sio, uint8_t *sif, int len);

extern void mtp2_command(uint8_t e1_no, uint8_t command);

extern void q921_start(mtp2_t *m);

extern int q921_receive(mtp2_t *m, q921_h *h, int len);

extern void q921_pick_frame(mtp2_t *m);

extern int q921_transmit_iframe(uint8_t e1_no, void *buf, int len /*, int cr*/);

extern int q921_transmit_uiframe(uint8_t e1_no, void *buf, int len);

extern void q921_cleanup(mtp2_t *m);

extern void send_ccs_msg(uint8_t e1_no, uint8_t send_len);

extern void rv_ccs_byte(uint8_t e1_no, uint8_t data);

extern void check_ccs_msg(uint8_t e1_no);

extern void bad_msg_rev(uint8_t e1_no, uint8_t err);

extern uint8_t read_l2_status(int e1_no);

extern void e1_port_init(int e1_no);


//extern void ds26518_send_sio_test(void);
extern void init_mtp2_mem(void);

extern uint8_t is_ccs_port(int e1_no);

extern void start_mtp2_process(int e1_no);

extern void check_memory(void);

//#define LOCK_MTP2_CORE()	sys_mutex_lock(&lock_mtp_core)
//#define UNLOCK_MTP2_CORE()  sys_mutex_unlock(&lock_mtp_core)

#endif
