
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

static card_heart_t  hb_msg;

void send_ss7_test_msg(void);
void send_isdn_test_msg(void);

void hb_msg_init(void);

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
    hb_msg_init();
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

void send_lsin_to_msc(u8_t e1_no)
{
    struct msc_msg lsin_msg;

    lsin_msg.m_head.sio = OTHER_SIO;
    lsin_msg.m_head.msg_type = 0x7;
    lsin_msg.src_slot = e1_no << 5;
    lsin_msg.decode_type = 0x3;
    lsin_msg.digit = e1_no;
    lsin_msg.m_head.msu_len = MAPB_HEAD_SIZE + OTHER_MSG_CONTENT_SIZE + 32 - 1; // not include itself.

    for (u8_t i = 0; i < 32; i++)
    {
        lsin_msg.other[i] = slot_params[(e1_no << 5) + i].ls_in;
    }

    send_other_msg((struct other_msg *)&lsin_msg, 32);
}

void send_mfc_num_to_msc(void)
{
    struct msc_msg mfc_msg;

    mfc_msg.m_head.sio = OTHER_SIO;
    mfc_msg.m_head.msg_type = 0x7;
    mfc_msg.src_slot = (7 << 5);
    mfc_msg.dst_id = card_id & 0x10;
    mfc_msg.decode_type = 0x5;
    mfc_msg.digit = 1;

    send_other_msg((struct other_msg *)&mfc_msg, 0);
}

void send_mfc_par_to_msc(u8_t e1_no)
{
    struct msc_msg mfc_msg;

    mfc_msg.m_head.sio = OTHER_SIO;
    mfc_msg.m_head.msg_type = 0x7;
    mfc_msg.src_slot = (e1_no << 5);
    mfc_msg.dst_id = 0;
    mfc_msg.decode_type = 0x4;
    mfc_msg.digit = e1_no;

    for (u8_t i = 0; i < 32; i++)
    {
        mfc_msg.other[i] = slot_params[(e1_no << 5) + i].mfc_value;
    }

    send_other_msg((struct other_msg *)&mfc_msg, 32);
}

void send_dtmf_to_msc(u8_t slot, u8_t dtmf)
{
    struct msc_msg dtmf_msg;

    dtmf_msg.m_head.sio = OTHER_SIO;
    dtmf_msg.m_head.msg_type = 0x7;
    dtmf_msg.src_slot = slot;
    dtmf_msg.dst_id = slot_params[slot].dmodule_ctone;
    dtmf_msg.dst_slot = slot_params[slot].dslot_ctone;
    dtmf_msg.decode_type = 0x2;
    dtmf_msg.digit = dtmf;

    send_other_msg((struct other_msg *)&dtmf_msg, 0);
}

#define CONNECT_TONE_FLAG   1
#define CONNECT_DTMF_FLAG   2
#define DECODE_DTMF_FLAG    3

static u8_t *get_tone_cadence(u8_t index)
{
    u8_t *tone = e1_params.tone_cadence0;
    return (tone + (u16_t)(index * 18));
}

/* Run at 50ms schedule. */ 
void connect_tone_proc(u8_t st_slot, u8_t end_slot)
{
    u8_t toneno, current_delay, cadence_byte;
    u8_t cadence_bit, cadence;
    u8_t dslot, con_port, con_slot, code;
    u8_t *tone;

    for (u8_t slot = st_slot; slot < end_slot; slot++) {
        switch (slot_params[slot].connect_tone_flag & 0x7) {
        case CONNECT_TONE_FLAG:
            current_delay = slot_params[slot].ct_delay;
            toneno = (slot_params[slot].connect_tone_flag >> 4) & 0x7;
            tone = get_tone_cadence(toneno);

            cadence_byte = current_delay >> 3;
            cadence_bit = current_delay & 0x7;
            cadence = (tone[2 + cadence_byte]) >> (7 - cadence_bit);
            dslot = slot_params[slot].dslot_ctone;

            con_port = (dslot >> 5) + (slot_params[slot].dmodule_ctone & 0xf) << 3;
            con_slot = dslot & 0x1f;

            if (cadence & 1) {
                con_port = TONE_E1;
                con_slot = tone[0] & 0x1f;
            } 

            if (con_port == TONE_E1 && con_slot ==  TONE_SILENT) {
                connect_tone(slot & 0x1f, slot >> 5, TONE_SILENT, TONE_STREAM);
            } else {
                connect_tone(slot & 0x1f, slot >> 5, con_slot, TONE_STREAM); // ???
            }
            slot_params[slot].ct_delay++;
            if (slot_params[slot].connect_time ==  0)  {
                slot_params[slot].ct_delay = 0;
                slot_params[slot].connect_tone_flag = 0xF0;
                if (con_port == TONE_E1 && con_slot == TONE_SILENT) {
                    connect_tone(slot & 0x1f, slot >> 5, TONE_SILENT, TONE_STREAM);
                } else {
                    connect_tone(slot & 0x1f, slot >> 5, con_slot, TONE_STREAM); //???
                }
            } else if (slot_params[slot].ct_delay >= tone[1]) {
                slot_params[slot].ct_delay = 0;
            }
            slot_params[slot].connect_time--;
            break;
        case CONNECT_DTMF_FLAG:
            toneno = (slot_params[slot].connect_tone_flag >> 4);
            if (slot_params[slot].dtmf_mark_delay > 0) {
                connect_tone(slot & 0x1f, slot >> 5, toneno, TONE_STREAM);
                slot_params[slot].dtmf_mark_delay--;
            } else {
                if (slot_params[slot].dtmf_space_delay == 0) {
                    dslot = slot_params[slot].dslot_ctone;
                    slot_params[slot].connect_tone_flag = 0xF0;

                    con_port = (dslot >> 5) + ((slot_params[slot].dmodule_ctone & 0xf) << 3);
                    con_slot = dslot & 0x1f;

                    if (con_port == TONE_E1 && con_slot == TONE_SILENT) {
                        connect_tone(slot & 0x1f, slot >> 5, TONE_SILENT, TONE_STREAM);
                    } else {
                        connect_tone(slot & 0x1f, slot >> 5, con_slot, TONE_STREAM); //???
                    }
                } else {
                    connect_tone(slot & 0x1f, slot >> 5, TONE_SILENT, TONE_STREAM);
                    slot_params[slot].dtmf_space_delay--;
                }
            }
            break;
        case DECODE_DTMF_FLAG:
            code = read_dtmf(slot);
            slot_params[slot].connect_tone_flag = (slot_params[slot].connect_tone_flag & 0x3) |
                                                  (code << 3);
            switch (slot_params[slot].echo_state & 0x3) {
            case 0:
                if (code >= 0x11 && code <= 0x1C) {
                    code &= 0xF;
                    code = (code == 0xA) ? 0 : code;
                    send_dtmf_to_msc(slot, code);
                    slot_params[slot].echo_state = 1;
                } 
                break;
            case 1:
                if ((code & 0x7F) == 0) {
                    slot_params[slot].echo_state = 0;
                }
                break;
            }
            break;
        default:
            break;
        }
    }
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

    send_trap_msg(&hb_msg, dst_flag);
}

/* 20ms period */
void ls_scan(int e1_no)
{
    e1_no &= 0x7;

    if (!CHN_NO1_PORT_ENABLE(e1_no) || !E1_PORT_ENABLE(e1_no)) {
        return;
    }
    if ((ram_params.e1_l1_alarm >> e1_no) & 1) {
        return;
    }

    u32_t change = check_rx_change(e1_no);
    if (change == 0) {
        return;
    }

    for (u8_t i = 1; i < 32; i++) {
        if ((change >> i) & 1) {
            slot_params[(e1_no << 5) + i].ls_in = read_rx_abcd(e1_no, i);
        }
    }

    send_lsin_to_msc(e1_no);
}

/* run at 50ms period */
u8_t alarm_state[16];
u8_t alarm_code[16];
u8_t component_id[16];
u8_t need_alarm_omc[16];


void alarm_fsm(u8_t proc)
{
    u8_t cl;
    u8_t ii, jj;

    switch (alarm_state[proc] & 1) {
    case 0: //normal 
        if (need_alarm_omc[proc] == 1) {
            hb_msg.cp_id = component_id[proc];
            hb_msg.alarm_code = 0;
            need_alarm_omc[proc] = 0;
            send_card_heartbeat(2); //send to omc
        }
        if (alarm_code[proc] != IDLE) {
            hb_msg.cp_id = component_id[proc];
            hb_msg.alarm_code = alarm_code[proc];
            send_card_heartbeat(plat_no); // send to sn
            need_alarm_omc[proc] = 1;
            alarm_state[proc] = 1;
        }
        break;
    case 1: // abnormal
        if (need_alarm_omc[proc] == 1) {
            hb_msg.cp_id = component_id[proc];
            hb_msg.alarm_code = alarm_code[proc];
            need_alarm_omc[proc] = 0;
            send_card_heartbeat(2); // to omc
        }
        if (alarm_code[proc] == IDLE) {
            hb_msg.cp_id = component_id[proc];
            hb_msg.alarm_code = 0;
            send_card_heartbeat(plat_no); // to sn
            alarm_code[proc] = 0;
        }
        break;
    }

    cl = 0xF;
    if (proc < 8) {
        /* E1 LED */
        component_id[proc] = proc + 0x10;
        if (((e1_params.e1_enable[card_id & 0xF] >> proc) & 1) == 0) {
            cl = 0x8; /* Gray */
            if (alarm_code[proc] != IDLE) {
                alarm_code[proc] = IDLE;
            } 
        } else if (((ram_params.e1_l1_alarm >> proc) & 1) == 1) {
            alarm_code[proc] = 1;
            cl = 0xB;  /* Red */
        } else if (((ram_params.e1_l2_alarm >> proc) & 1) != 1) {
            cl = 0xA; /* Yellow */
        } else {
            if (alarm_code[proc] != IDLE) {
                alarm_code[proc] = IDLE;
            }
            cl = 0x9; /* Green */
        }
    } else if (proc < 11) {
        if (proc == 9) {  /* Master clock */
            hb_msg.omcled[0] = 1;
        } else if (proc == 10) { /* DPLL */
            //Todo:
            component_id[proc] = 0x40;
            //如果锁时钟失败
            //alarm_code[proc] = lock_alarm;
            //如果锁时钟成功
            alarm_code[proc] = IDLE;
        }
    } else if (proc < 14) {
        if ((card_id & (1 << (proc - 6))) == 0) {
            if (alarm_code[proc] != IDLE) {
                alarm_code[proc] = IDLE;
            }
            cl = 9;
        }
    }

    if (proc < 14) {
        ii = (proc >> 1) + 1;
        jj = hb_msg.omcled[ii];
        if (proc & 1) {
            jj = (jj & 0xF0) + cl;
        } else {
            jj = (jj & 0x0F) + (cl << 4);
        }
        hb_msg.omcled[ii] = jj;
    }

    if ((card_id & 0xF) > 1) {
        hb_msg.omcled[6] = 0xff;
        hb_msg.omcled[7] = 0xff;
    } else {
        hb_msg.omcled[6] = 0xF9;
    }
}

void period_50ms_proc(void)
{
    
}
