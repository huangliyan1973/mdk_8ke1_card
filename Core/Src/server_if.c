
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lwip/sys.h"
#include "lwip.h"
#include "lwip/api.h"

#include "main.h"
#include "mtp.h"
#include "eeprom.h"
#include "card_debug.h"
#include "server_interface.h"
#include "ds26518.h"
#include "zl50020.h"

extern ip4_addr_t local_addr;

static struct netconn *ss7_conn;
static struct netconn *isdn_conn;
//static struct netconn *other_conn;

void send_ss7_test_msg(void);
void send_isdn_test_msg(void);

static void other_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr)
{
    struct other_msg  *ot_msg = (struct other_msg *)p->payload;
    
    u8_t dst_port, dst_slot, src_port, src_slot;
    u8_t toneno, group, old_group, i;

    dst_port = ((ot_msg->dst_id & 0xF) << 3) | (ot_msg->dst_slot >> 5);
    dst_slot = ot_msg->dst_slot & 0x1F;
    src_port = ot_msg->src_slot >> 5;
    src_slot = ot_msg->src_slot & 0x1F;

    switch (ot_msg->m_head.msg_type) {
        case CON_TIME_SLOT:
            ds26518_e1_slot_enable(src_port, src_slot, VOICE_ACTIVE);
            if (dst_port == TONE_E1) {
                connect_tone(src_slot, src_port, dst_slot, TONE_STREAM);
            } else {
                connect_slot(src_slot, src_port, dst_slot, dst_port);
            }
            break;
        case CON_TONE:
            //ds26518_e1_slot_enable(src_port, src_slot, VOICE_ACTIVE);
            //connect_tone(src_slot, src_port, 0, TONE_STREAM);
            toneno = e1_params.reason_to_tone[ot_msg->tone_no & 0x0F] & 0xF; /* tone0, tone1,...tone7 */
            slot_params[ot_msg->src_slot].connect_tone_flag = (toneno << 4) + 1;
            slot_params[ot_msg->src_slot].dmodule_ctone = ot_msg->dst_id; /* destination module when connect tone */
            slot_params[ot_msg->src_slot].dslot_ctone = ot_msg->dst_slot; /* destination slot when connect tone */
            slot_params[ot_msg->src_slot].connect_time = ot_msg->playtimes * 20;
            break;
        case CON_DTMF:
            toneno = ot_msg->tone_no;
            src_slot = ot_msg->src_slot;
            slot_params[src_slot].connect_tone_flag = (toneno << 4) + 2;
            slot_params[src_slot].dmodule_ctone = ot_msg->dst_id;
            slot_params[src_slot].dslot_ctone = ot_msg->dst_slot;
            slot_params[src_slot].dtmf_mark_delay = 39;
            slot_params[src_slot].dtmf_space_delay = 19;
            break;
        case CON_CONN_GRP:
            if (!ram_params.conf_module_installed) {
                return;
            }
            group = ot_msg->tone_no % 81;
            old_group = slot_params[ot_msg->src_slot].port_to_group;
            if (old_group != IDLE) {
                if (group_user[old_group] > 0) {
                    group_user[old_group]--;
                } else {
                    group_user[old_group] = IDLE;
                }
                m34116_disconnect(ot_msg->src_slot);
            }

            slot_params[ot_msg->src_slot].port_to_group = group;
            group_user[group]++;
            if (group_user[group] == 1) {
                m34116_conf_connect((group % 10) + 1, 0, 2, 0, 0, ot_msg->src_slot, 0, 0);
            } else {
                toneno = ((group_user[group] >> 2) * 3 + 2) & 0xF;
                src_port = (src_port >> 5) << 5;
                for (i = src_port; i < src_port + 32; i++) {
                    if (slot_params[ot_msg->src_slot].port_to_group == group) {
                        m34116_conf_connect((group % 10) + 1, 0, toneno, 1, 2, ot_msg->src_slot, 0, 0);
                    }
                }
            }
            break;
        case CON_DISC_GRP:
            group = ot_msg->tone_no % 80;
            old_group = slot_params[ot_msg->src_slot].port_to_group;
            if (old_group != IDLE) {
                if (group_user[old_group] > 0) {
                    group_user[old_group]--;
                }
            }
            slot_params[ot_msg->src_slot].port_to_group = IDLE;
            connect_tone(src_slot, src_port, TONE_SILENT, TONE_STREAM);
            m34116_disconnect(ot_msg->src_slot);
            break;
        case CON_DEC_DTMF:
            slot_params[ot_msg->src_slot].dmodule_ctone = ot_msg->dst_id; /* destination module when connect tone */
            slot_params[ot_msg->src_slot].dslot_ctone = ot_msg->dst_slot; /* destination slot when connect tone */

            if (ot_msg->tone_no & 1) {
                /* Stop decode. */
            } else {
                /* Start decode */                
            }
            break;
        case CON_DEC_MFC:
            
            break;
        default:
            break;
    }
}

#if 0
static void other_netconn_thread(void *arg)
{
    struct netconn *conn = NULL;
    struct netbuf *buf = NULL;
    err_t err;

    LWIP_UNUSED_ARG(arg);

    conn = netconn_new(NETCONN_UDP);
    netconn_bind(conn, IP4_ADDR_ANY, OTHER_UDP_PORT);

    CARD_ERROR("other_netconn: invalid conn", (conn != NULL), return;);
    other_conn = conn;

    do {
        err = netconn_recv(conn, &buf);
        if (err == ERR_OK) {
            other_receive(conn, buf->p, &buf->addr, buf->port);
        }

        if (buf != NULL) {
            netbuf_delete(buf);
        }
    } while (1);
}
#endif

static void isdn_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr, u16_t port)
{
    LWIP_UNUSED_ARG(conn);

    //u8_t buf[512];
    struct isdn_msg *isdnmsg = (struct isdn_msg *)p->payload;
    
    u8_t pd = isdnmsg->msg.l3msg.pd;
    u8_t e1_no = isdnmsg->e1_no;

    if ((e1_no >> 3) != card_id) {
        CARD_DEBUGF(MSG_DEBUG, ("out of isdn msg range e1_no = %d, card_id = %d\n", e1_no, card_id));
        return;
    }

    if (pd == ISDN_PD) {
#if 0
        memset(buf, 0, 512);
        buf[0] = isdnmsg->msg.l3msg.pd;
        buf[1] = isdnmsg->msg.l3msg.cr_len;
        buf[2] = isdnmsg->msg.l3msg.callref[0];
        buf[3] = isdnmsg->msg.l3msg.callref[1];
        buf[4] = isdnmsg->msg.l3msg.msgtype;
        memcpy(&buf[5], isdnmsg->msg.l3msg.l3content, isdnmsg->msg_len -5);
        q921_transmit_iframe(e1_no & 0x7, buf, isdnmsg->msg_len);
#endif
        q921_transmit_iframe(e1_no & 0x7, (void *)&isdnmsg->msg.l3msg.pd, isdnmsg->msg_len);
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

    CARD_ERROR("isdn_netconn: invalid conn", (conn != NULL), return;);
    isdn_conn = conn;

    //for test.
    send_isdn_test_msg();
    
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

static void ss7_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr, u16_t port)
{
    struct ss7_msg *rev_msg = (struct ss7_msg *)p->payload;

    u8_t sio = rev_msg->msg.sio;
    u8_t e1_no = rev_msg->msg.e1_no;

    if (sio == OTHER_SIO) {
        /* other command for 8KE1 */
        other_receive(conn, p, src_addr);
    } else if (sio == MTP2_COMMAND_SIO) {
        /* MTP2 command */
        if ((e1_no >> 3) != card_id) {
            CARD_DEBUGF(MSG_DEBUG, ("out of range e1_no = %d, card_id = %d\n", e1_no, card_id));
            return;
        }
        u8_t command = rev_msg->msg.msg[1];
        mtp2_command(e1_no & 0x7, command);
    } else if ((sio & 0x0F) < 6) {
        /* TUP, ISUP, SCCP */
        if ((e1_no >> 3) != card_id) {
            CARD_DEBUGF(MSG_DEBUG, ("out of range e1_no = %d, card_id = %d\n", e1_no, card_id));
            return;
        }
        mtp2_queue_msu(e1_no & 0x7, sio, rev_msg->msg.msg, rev_msg->msg.len);
    } else {
        CARD_DEBUGF(MSG_DEBUG, ("Error sio = %2x\n", sio));
    }
}

static void ss7_netconn_thread(void *arg)
{
  struct netconn *conn;
  struct netbuf *buf;
  err_t err;
  LWIP_UNUSED_ARG(arg);

  conn = netconn_new(NETCONN_UDP);
  netconn_bind(conn, IP_ADDR_ANY, SS7_UDP_PORT);

  CARD_ERROR("ss7_netconn: invalid conn", (conn != NULL), return;);
  ss7_conn = conn;

  //for test
  send_ss7_test_msg();
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

#define SS7_STACK_SIZE      128*4

void server_interface_init(void)
{   
    sys_thread_new("ss7_netconn", ss7_netconn_thread, NULL, SS7_STACK_SIZE, osPriorityNormal);
    sys_thread_new("isdn_netconn", isdn_netconn_thread, NULL, SS7_STACK_SIZE, osPriorityNormal);
    //sys_thread_new("other_netconn", other_netconn_thread, NULL, SS7_STACK_SIZE, osPriorityNormal);
}

static void init_msg_ip_head(union updmsg *msg, ip4_addr_t *dst_addr, u16_t dst_port)
{
    if (plat_no) {
        ip4_addr_copy(*dst_addr, sn1);
    } else {
        ip4_addr_copy(*dst_addr, sn0);
    }

    msg->ss7.ip_head.msgDstIp = dst_addr->addr;
    msg->ss7.ip_head.msgSrcIp = local_addr.addr;

    msg->ss7.ip_head.msgDstPort = PP_HTONS(dst_port);
    msg->ss7.ip_head.msgSrcPort = PP_HTONS(dst_port);

    msg->ss7.ip_head.msgBroadcast = 0;
}

void send_ss7_msg(u8_t link_no, u8_t *buf, u8_t len)
{
    ip4_addr_t dst_addr;

    struct netbuf *net_buf = netbuf_new();
    u16_t tot_len = sizeof(struct ss7_msg) + len + 3;

    struct ss7_msg *sig_msg = (struct ss7_msg *)netbuf_alloc(net_buf, tot_len);
    if (sig_msg == NULL) {
        CARD_DEBUGF(MSG_DEBUG, ("failed to alloc mem for ss7_msg\n"));
        return;
    }
    
    init_msg_ip_head((union updmsg *)sig_msg, &dst_addr, SS7_UDP_PORT);

    sig_msg->ip_head.msgLens = PP_HTONS(len + 2);

    sig_msg->msg.e1_no = link_no + (card_id << 3);
    sig_msg->msg.len = len;
    sig_msg->msg.sio = buf[0];
    memcpy(sig_msg->msg.msg, &buf[1], len);

    if (netconn_sendto(ss7_conn, net_buf, &dst_addr, SS7_UDP_PORT) != ERR_OK) {
        CARD_DEBUGF(MSG_DEBUG, ("send ss7 msg failed.\n"));
    }

    netbuf_delete(net_buf);
}

/* len = msg->m_head.msu_len;
*/
void send_other_msg(struct other_msg *msg, u8_t len)
{
    ip4_addr_t dst_addr;

    struct netbuf *net_buf = netbuf_new();
    u16_t tot_len = sizeof(struct ip_head) + len;

    struct other_msg *send_msg = (struct other_msg *)netbuf_alloc(net_buf, tot_len);
    if (send_msg == NULL)
    {
        CARD_DEBUGF(MSG_DEBUG, ("failed to alloc mem for other_msg\n"));
        return;
    }

    init_msg_ip_head((union updmsg *)send_msg, &dst_addr, OTHER_UDP_PORT);

    send_msg->ip_head.msgLens = PP_HTONS(msg->m_head.msu_len + 1);

    memcpy((void *)&send_msg->m_head.msu_len, (void *)&msg->m_head.msu_len, len+1);

    if (netconn_sendto(ss7_conn, net_buf, &dst_addr, OTHER_UDP_PORT) != ERR_OK) {
        CARD_DEBUGF(MSG_DEBUG, ("send other msg failed.\n"));
    }

    netbuf_delete(net_buf);
}

void send_isdn_msg(u8_t link_no, u8_t *buf, u8_t len)
{
    ip4_addr_t dst_addr;

    struct netbuf *net_buf = netbuf_new();
    u16_t tot_len = sizeof(struct isdn_msg) + len - 3;

    struct isdn_msg *isdnmsg = (struct isdn_msg *)netbuf_alloc(net_buf, tot_len);
    if (isdnmsg == NULL) {
        CARD_DEBUGF(MSG_DEBUG, ("failed to alloc mem for isdn_msg\n"));
        return;
    }
    
    init_msg_ip_head((union updmsg *)isdnmsg, &dst_addr, ISDN_UDP_PORT);

    isdnmsg->ip_head.msgLens = PP_HTONS(len + 2);

    isdnmsg->e1_no = link_no + (card_id << 3);
    isdnmsg->msg_len = len;

    memcpy((void *)&isdnmsg->msg.l3msg.pd, (void *)buf, len);

    if (netconn_sendto(isdn_conn, net_buf, &dst_addr, ISDN_UDP_PORT) != ERR_OK) {
        CARD_DEBUGF(MSG_DEBUG, ("send isdn msg failed.\n"));
    }

    netbuf_delete(net_buf);
}

void send_ss7_test_msg(void)
{
    u8_t test_buf[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    send_ss7_msg(3, test_buf, sizeof(test_buf)/test_buf[0]);
    
/*    
    HAL_Delay(4);
    struct other_msg t2_msg;
    t2_msg.m_head.sio = 0x87;
    t2_msg.m_head.msg_type = 0x3;
    t2_msg.tone_no = 1;
    t2_msg.dst_id = 2;
    t2_msg.dst_slot = 3;
    t2_msg.src_slot = 4;
    t2_msg.playtimes = 20;
    
    send_other_msg(&t2_msg, sizeof(t2_msg));
*/
}

void send_isdn_test_msg(void)
{
    u8_t test_buf[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    send_isdn_msg(2, test_buf, sizeof(test_buf)/test_buf[0]);
}

