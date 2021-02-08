
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lwip/sys.h"
#include "lwip.h"
#include "lwip/api.h"

#include "main.h"
#include "FreeRTOS.h"
#include "mtp.h"
#include "eeprom.h"
#include "server_interface.h"
#include "ds26518.h"
#include "zl50020.h"
#include "sched.h"
#include "mtp2_def.h"
#include "ip4_addr.h"

#define LOG_TAG              "server"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

static struct netconn *ss7_conn;
static struct netconn *isdn_conn;
//static struct netconn *other_conn;

extern u8_t card_id;

card_heart_t  hb_msg;
mtp2_heart_t mtp2_heart_msg;

void send_ss7_test_msg(void);
void send_isdn_test_msg(void);

static void other_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr)
{
    
}

#if 0
static void isdn_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr, u16_t port)
{
    LWIP_UNUSED_ARG(conn);

    struct isdn_msg *isdnmsg = (struct isdn_msg *)p->payload;
    
    u8_t pd = isdnmsg->msg.l3msg.pd;
    u8_t e1_no = isdnmsg->e1_no;

    if ((e1_no >> 3) != card_id) {
        printf("out of isdn msg range e1_no = %d, card_id = %d\n", e1_no, card_id);
        return;
    }

    if (pd == ISDN_PD) {

    }
}

static void isdn_netconn_thread(void *arg)
{
    struct netconn *conn = NULL;
    struct netbuf *buf = NULL;
    err_t err;

    LWIP_UNUSED_ARG(arg);

    conn = netconn_new(NETCONN_UDP);
    netconn_bind(conn, IP_ADDR_ANY, ISDN_UDP_PORT);

    isdn_conn = conn;

	printf("ISDN thread Start!\n");
    
    do {
        err = netconn_recv(conn, &buf);
        if (err == ERR_OK){
            isdn_receive(conn, buf->p, &buf->addr, buf->port);
        }

        if (buf != NULL){
            netbuf_delete(buf);
        }
    } while (1);
}
#endif

static void ss7_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr, u16_t port)
{
    struct ss7_head *rev_msg = (struct ss7_head *)p->payload;

    //u8_t sio = rev_msg->sio;
    //u8_t e1_no = rev_msg->e1_no;

	LOG_I("server msg: srcip: %s, len: %d", ip4addr_ntoa((const ip4_addr_t *)src_addr), p->tot_len);
    //LOG_HEX("server", 16, (u8_t *)p, p->tot_len);
}

static void ss7_netconn_thread(void *arg)
{
  struct netconn *conn;
  struct netbuf *buf;
  err_t err;
  LWIP_UNUSED_ARG(arg);

  conn = netconn_new(NETCONN_UDP);
  netconn_bind(conn, IP_ADDR_ANY, SS7_UDP_PORT);

  ss7_conn = conn;
	
  printf("SS7 thread Start!\n");

  do {
    err = netconn_recv(conn, &buf);

    if (err == ERR_OK) {
      ss7_receive(conn, buf->p, &buf->addr, buf->port);
    }

    if (buf != NULL) {
      netbuf_delete(buf);
    }
  } while (1);
}

#define SS7_STACK_SIZE      256*4

void server_interface_init(void)
{   
    //hb_msg_init();
    //period_10s_proc(NULL);
    sys_thread_new("ss7_netconn", ss7_netconn_thread, NULL, SS7_STACK_SIZE, osPriorityNormal);
    //sys_thread_new("isdn_netconn", isdn_netconn_thread, NULL, SS7_STACK_SIZE, osPriorityNormal);
    
}


void send_ss7_msg(u8_t link_no, u8_t *buf, u8_t len)
{
    
}

/* len = msg->m_head.msu_len;
*/
void send_other_msg(struct other_msg *msg, u8_t len)
{
    
}

void send_isdn_msg(u8_t link_no, u8_t *buf, u8_t len)
{
    
}

void send_ss7_test_msg(void)
{
    u8_t test_buf[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    send_ss7_msg(3, test_buf, sizeof(test_buf)/test_buf[0]);
    
}

void send_isdn_test_msg(void)
{
    u8_t test_buf[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    send_isdn_msg(2, test_buf, sizeof(test_buf)/test_buf[0]);
}

void send_lsin_to_msc(u8_t e1_no)
{
    
}

void send_mfc_num_to_msc(void)
{
   
}

void send_mfc_par_to_msc(u8_t e1_no)
{
    
}

void send_dtmf_to_msc(u8_t slot, u8_t dtmf)
{
    
}

void update_no1_e1(u8_t new_value)
{
   
}


void hb_msg_init(void)
{
    hb_msg.hb_version[0] = 9;
    hb_msg.hb_version[1] = 0;
    hb_msg.hb_version[2] = 0;
    hb_msg.component.cpu_loading = 19;
}

void send_card_heartbeat(u8_t dst_flag)
{

    hb_msg.sys_id = (card_id >> 4) & 1;
    hb_msg.subsys_id = card_id & 0xF;
    hb_msg.timestamp = PP_HTONL(ram_params.timestamp);

    hb_msg.component.e1_install = e1_params.e1_enable[card_id & 0xF];
    hb_msg.component.e1_l2_install_fg = 0;
    hb_msg.component.e1_l2_alarm = ram_params.e1_l2_alarm;
    hb_msg.component.e1_l1_alarm = ram_params.e1_l1_alarm;

    send_trap_msg(dst_flag);
}

void mtp2_heart_msg_init(void)
{
    mtp2_heart_msg.sys_id = plat_no;
    mtp2_heart_msg.subsys_id = card_id & 0x0F;
    //u8_t dst_ip = (plat_no == 0 ? 98 : 99);
    memset(mtp2_heart_msg.dst_ip, 0xff, 8);
    mtp2_heart_msg.mtp2_soft_version = 0x50;
}

void link_in_service(int e1_no)
{
    mtp2_heart_msg.timestamp = ram_params.timestamp;
    mtp2_heart_msg.led_ctl_code[e1_no & 7] = LED_IN_SERVICE;
    mtp2_heart_msg.alarm_component = e1_no & 7;

    u8_t dst_ip = (plat_no == 0 ? 98 : 99);
    memset(mtp2_heart_msg.dst_ip, dst_ip, 8);

    mtp2_heart_msg.alarm_code = ALARM_IN_SERVICE;
    mtp2_heart_msg.l1_status = ram_params.e1_l1_alarm;
    mtp2_heart_msg.l2_status[e1_no & 7] = ram_params.e1_l2_alarm;
    mtp2_heart_msg.e1_port_type = e1_params.e1_port_type[card_id & 0x0f];
    mtp2_heart_msg.is_NT = e1_params.isdn_port_type[card_id & 0x0f];
    mtp2_heart_msg.mtp2_mode = e1_params.mtp2_error_check[card_id & 0x0f];

    send_mtp2_trap_msg();    
}

void link_outof_service(int e1_no, u8_t alarm_code)
{
    mtp2_heart_msg.timestamp = ram_params.timestamp;
    mtp2_heart_msg.led_ctl_code[e1_no & 7] = LED_ALIGNMENT;
    mtp2_heart_msg.alarm_component = e1_no & 7;

    mtp2_heart_msg.l1_status = ram_params.e1_l1_alarm;
    mtp2_heart_msg.l2_status[e1_no & 7] = (ram_params.e1_l2_alarm >> e1_no) & 1;
    //mtp2_heart_msg.retrieved_bsnt =  0;

    send_mtp2_trap_msg();
}

/* 20ms period */
void ls_scan(int e1_no)
{
    
}

void mfc_scan(void)
{
    
}

void update_e1_l1_status(void)
{
   
}

void update_e1_l2_status(void)
{
   
}

void period_50ms_proc(void *arg)
{
    (void)arg;

	//printf("50ms proc start!\n");
    sched_timeout(50, period_50ms_proc, NULL);
}

void period_20ms_proc(void *arg)
{
    (void)arg;

    sched_timeout(20, period_20ms_proc, NULL);
}

void period_500ms_proc(void *arg)
{
    (void)arg;

    sched_timeout(500, period_500ms_proc, NULL);
}


void period_10s_proc(void *arg)
{
    static u8_t pingpang = TO_OMC;

    (void)arg;
#if 0
    if (pingpang == TO_OMC) {
        send_card_heartbeat(pingpang);
        pingpang = plat_no;
    } else {
        send_card_heartbeat(pingpang);
        pingpang = TO_OMC;
    }
#endif
    send_mtp2_trap_msg();
    LOG_I("perioad 10s timeout");

    //print_task();
    sched_timeout(10000, period_10s_proc, NULL);
}

void start_period_proc(void)
{
    period_50ms_proc(NULL);

    period_20ms_proc(NULL);

    period_500ms_proc(NULL);

    //period_10s_proc(NULL);
}
