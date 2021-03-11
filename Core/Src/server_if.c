
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
//static struct netconn *isdn_conn;
//static struct netconn *other_conn;

extern u8_t card_id;

card_heart_t  hb_msg;
mtp2_heart_t mtp2_heart_msg;

static void send_mfc_num_to_msc(void);

static char *get_other_msg_type(struct other_msg *ot_msg)
{
    switch (ot_msg->m_head.msg_type) {
        case CON_TIME_SLOT:
            return "Connect slot";
        case CON_TONE:
            return "Connect tone";
        case CON_DTMF:
            return "Connect Dtmf";
        case CON_CONN_GRP:
            return "Conference";
        case CON_DISC_GRP:
            return "Dismiss Con";
        case CON_DEC_DTMF:
            return "Decode Dtmf";
        case CON_DEC_MFC:
            if (ot_msg->playtimes == 0) {
                return "Send ABCD";
            }
            return "Query MFC";
        default:
            return "Unknown";
    }
}

static void other_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr)
{
    struct other_msg  *ot_msg = (struct other_msg *)p->payload;
    
    u8_t dst_port, dst_slot, src_port, src_slot;
    u8_t toneno, group, old_group, i;

    dst_port = ((ot_msg->dst_id & 0xF) << 3) | (ot_msg->dst_slot >> 5);
    dst_slot = ot_msg->dst_slot & 0x1F;
    src_port = ot_msg->src_slot >> 5;
    src_slot = ot_msg->src_slot & 0x1F;

    LOG_I("SN==>%s: %x[%x] <-- %x[%x]  tone_no=%d  playtime=%d", get_other_msg_type(ot_msg),
            src_slot, src_port, dst_slot, dst_port, ot_msg->tone_no, ot_msg->playtimes);
    
    //LOG_HEX("", 16, (u8_t *)p->payload, p->tot_len);

    switch (ot_msg->m_head.msg_type) {
        case CON_TIME_SLOT:
            if ((slot_params[ot_msg->src_slot].connect_tone_flag & 0xf) > 0) {
                slot_params[ot_msg->src_slot].connect_tone_flag = 0xf0;
                slot_params[ot_msg->src_slot].ct_delay = 0;
            }
            ds26518_e1_slot_enable(src_port, src_slot, VOICE_ACTIVE);
            connect_slot(src_slot, src_port, dst_slot, dst_port);
            break;
        case CON_TONE:
            ds26518_e1_slot_enable(src_port, src_slot, VOICE_ACTIVE);
            connect_tone(src_slot, src_port, VOICE_450HZ_TONE);
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
            connect_slot(src_slot, src_port, TONE_SILENT, TONE_STREAM);
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
            if (ot_msg->playtimes == 1) {
                /* query mfc number */
                send_mfc_num_to_msc();
            } else if (ot_msg->playtimes == 0) {
                /* send line code */
                out_tx_abcd(src_port, src_slot, ot_msg->tone_no);
            }
            break;
        default:
            break;
    }
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

static char *rev_mtp2_msg_name(u8_t sio)
{
    if (sio == 1 || sio == 0) {
        return "SN:<==SNM";
    } else if (sio == 3) {
       return "SN:<==SCCP";
    } else if (sio == 4) {
        return "SN:<==TUP";
    } else if (sio == 5) {
        return "SN:<==ISUP";
    }

    return "SN:<==UNKOWN";
}

static char *rev_mtp3_msg_name(u8_t sio)
{
    if (sio == 1 || sio == 0) {
        return "SN:==>SNM";
    } else if (sio == 3) {
       return "SN:==>SCCP";
    } else if (sio == 4) {
        return "SN:==>TUP";
    } else if (sio == 5) {
        return "SN:==>ISUP";
    }

    return "SN:==>UNKOWN";
}

static void ss7_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr, u16_t port)
{
    struct server_msg *serv_msg = (struct server_msg *)p->payload;

	//LOG_I("server msg: srcip: %s, len: %d", ip4addr_ntoa((const ip4_addr_t *)src_addr), p->tot_len);
    //LOG_HEX("server", 16, (u8_t *)p, p->tot_len);

    u8_t sio = serv_msg->sio;
    u8_t e1_no = serv_msg->e1_no;

    if ((sio != OTHER_SIO && sio != MTP2_COMMAND_SIO)) {
        if ((e1_no >> 3) != card_id) {
            LOG_E("rev e1_no = %02X, card_id = %02X, can't match!", e1_no, card_id);
            LOG_HEX("d", 16, p->payload, p->tot_len);
            return;
        }
    }
    switch (sio) {
        case MTP2_COMMAND_SIO:
            if (serv_msg->msg.serv_comm.command == 0xFF) {
                set_mtp2_heart_msg_dstip(serv_msg->msg.serv_comm.dest_ip);
            } else {
                mtp2_command(e1_no & 7, serv_msg->msg.serv_comm.command);
            }
            //mtp2_heart_msg.dst_ip[e1_no & 7] = serv_msg->msg.serv_comm.server_ip;
            break;
        case OTHER_SIO:
            other_receive(conn, p, src_addr);
            break;
        case ISDN_SIO:
            q921_transmit_iframe(e1_no & 0x7, (void *)&serv_msg->sio, serv_msg->msg_len);
            break;
        default:
            if ((sio & 0xf) < 6) {
              LOG_HEX(rev_mtp3_msg_name(sio & 0xf), 16, serv_msg->msg.ss7.contents, serv_msg->msg_len-1);

                mtp2_queue_msu(e1_no & 0x7, sio, serv_msg->msg.ss7.contents, serv_msg->msg_len-1);
            }
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
    //init_r2_param();
    init_mfc_slot();
    sys_thread_new("ss7_netconn", ss7_netconn_thread, NULL, SS7_STACK_SIZE, osPriorityNormal);
    start_period_proc();
    //sys_thread_new("isdn_netconn", isdn_netconn_thread, NULL, SS7_STACK_SIZE, osPriorityNormal);
    
}


void send_ss7_msg(u8_t link_no, u8_t *buf, u8_t len)
{
    ip4_addr_t *dst_addr;
    
    if (ss7_conn == NULL)
        return;

    struct netbuf *net_buf = netbuf_new();
    u16_t tot_len = sizeof(struct ss7_msg) + len - 1; /* delete ss7_msg.contents[0] len */

    struct ss7_msg *sig_msg = (struct ss7_msg *)netbuf_alloc(net_buf, tot_len);
    if (sig_msg == NULL) {
        LOG_W("failed to alloc memory for SS7 mssage.");
        return;
    }

    sig_msg->msg_len = len;

    sig_msg->e1_no = link_no + (card_id << 3);

    memcpy(sig_msg->contents, buf, len);

    dst_addr = plat_no ? &sn1 : &sn0;
    
    if (netconn_sendto(ss7_conn, net_buf, dst_addr, SS7_UDP_PORT) != ERR_OK) {
        LOG_W("send ss7 msg failed.\n");
    }

    LOG_HEX(rev_mtp2_msg_name(buf[0] & 0xf), 16, (u8_t *)sig_msg, tot_len);
    
    netbuf_delete(net_buf);
}

static char * other_msg_type(struct other_msg *msg)
{
    switch (msg->tone_no)
    {
        case 0x3:
            return "LS_IN";
        case 0x4:
            return "MFC_IN";
        case 0x5:
            return "MFC_NUMBER";
        case 0x2:
            return "DTMF_IN";
        default:
            return "UNKOWN";
    }
}
/* len = msg->m_head.msu_len;
*/
void send_other_msg(struct other_msg *msg, u8_t len)
{
    ip4_addr_t *dst_addr;
    
    if (ss7_conn == NULL)
        return;

    struct netbuf *net_buf = netbuf_new();
    u16_t tot_len = sizeof(struct other_msg) + len;

    struct other_msg *send_msg = (struct other_msg *)netbuf_alloc(net_buf, tot_len);
    if (send_msg == NULL) {
        LOG_E("failed to alloc memory for control mssage");
        return;
    }

    memcpy((void *)send_msg, (void *)msg, tot_len);
    send_msg->m_head.msu_len = tot_len - 1;
    dst_addr = plat_no ? &sn1 : &sn0;

    if (netconn_sendto(ss7_conn, net_buf, dst_addr, OTHER_UDP_PORT) != ERR_OK) {
        LOG_W("send control msg failed.\n");
    }

    //LOG_HEX(other_msg_type(send_msg), 16, (u8_t *)send_msg, tot_len);
    
    netbuf_delete(net_buf);
}

void send_isdn_msg(u8_t link_no, u8_t *buf, u8_t len)
{
    ip4_addr_t *dst_addr;
    
    if (ss7_conn == NULL)
        return;

    struct netbuf *net_buf = netbuf_new();
    u16_t tot_len = sizeof(struct server_msg) + len - 2;

    struct server_msg *isdn_msg = (struct server_msg *)netbuf_alloc(net_buf, tot_len);
    if (isdn_msg == NULL) {
        LOG_E("failed to alloc memory for ISDN mssage");
        return;
    }

    isdn_msg->e1_no = link_no + (card_id << 3);
    isdn_msg->msg_len = len;

    memcpy((void *)&isdn_msg->sio, (void *)buf, len);
    dst_addr = plat_no ? &sn1 : &sn0;

    if (netconn_sendto(ss7_conn, net_buf, dst_addr, ISDN_UDP_PORT) != ERR_OK) {
        LOG_W("send ISDN msg failed.\n");
    }

    netbuf_delete(net_buf);
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

static void send_mfc_num_to_msc(void)
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

void update_no1_e1(u8_t new_value)
{
    u8_t nbit, obit;

    if (new_value != e1_params.no1_enable[card_id & 0x10]) {
        for (u8_t i = 0; i < 8; i++) {
            obit = (e1_params.no1_enable[card_id & 0x10] >> i) & 1;
            nbit = (new_value >> i) & 1;
            if (obit != nbit) {
                e1_port_init(i);
            }
        }    
    }

    e1_params.no1_enable[card_id & 0x10] = new_value;
}

#define CONNECT_TONE_FLAG   1
#define CONNECT_DTMF_FLAG   2
#define DECODE_DTMF_FLAG    3

/**
static u8_t *get_tone_cadence(u8_t index)
{
    u8_t *tone = e1_params.tone_cadence0;
    return (tone + (u16_t)(index * 18));
}
**/

/* Run at 50ms schedule. */ 
void connect_tone_proc(u8_t st_slot, u8_t end_slot)
{
    u8_t toneno;
    //u8_t cadence_bit, cadence, current_delay, cadence_byte;
    u8_t dslot, con_port, con_slot, code;
    //u8_t *tone;

    for (u8_t slot = st_slot; slot < end_slot; slot++) {
        switch (slot_params[slot].connect_tone_flag & 0x7) {
        case CONNECT_TONE_FLAG:
            tone_rt(slot);
#if 0
            current_delay = slot_params[slot].ct_delay;
            toneno = (slot_params[slot].connect_tone_flag >> 4) & 0x7;
            tone = get_tone_cadence(toneno);

            cadence_byte = current_delay >> 3;
            cadence_bit = current_delay & 0x7;
            cadence = (tone[2 + cadence_byte]) >> (7 - cadence_bit);
            dslot = slot_params[slot].dslot_ctone;

            con_port = (dslot >> 5) + ((slot_params[slot].dmodule_ctone & 0xf) << 3);
            con_slot = dslot & 0x1f;

            if (cadence & 1) {
                con_port = TONE_E1;
                con_slot = tone[0] & 0x1f;
            } 

            if (con_port == TONE_E1 && con_slot ==  TONE_SILENT) {
                ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_INACTIVE);
                connect_tone(slot & 0x1f, slot >> 5, TONE_SILENT, TONE_STREAM);
            } else {
                ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_ACTIVE);
                connect_tone(slot & 0x1f, slot >> 5, con_slot, TONE_STREAM); // ???
            }
            slot_params[slot].ct_delay++;
            if (slot_params[slot].connect_time ==  0)  {
                slot_params[slot].ct_delay = 0;
                slot_params[slot].connect_tone_flag = 0xF0;
                if (con_port == TONE_E1 && con_slot == TONE_SILENT) {
                    ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_INACTIVE);
                    connect_tone(slot & 0x1f, slot >> 5, TONE_SILENT, TONE_STREAM);
                } else {
                    ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_ACTIVE);
                    connect_tone(slot & 0x1f, slot >> 5, con_slot, TONE_STREAM); //???
                }
            } else if (slot_params[slot].ct_delay >= tone[1]) {
                slot_params[slot].ct_delay = 0;
            }
            slot_params[slot].connect_time--;
#endif
            break;
        case CONNECT_DTMF_FLAG:
            toneno = (slot_params[slot].connect_tone_flag >> 4);
            if (slot_params[slot].dtmf_mark_delay > 0) {
                connect_tone(slot & 0x1f, slot >> 5, toneno);
                slot_params[slot].dtmf_mark_delay--;
            } else {
                if (slot_params[slot].dtmf_space_delay == 0) {
                    dslot = slot_params[slot].dslot_ctone;
                    slot_params[slot].connect_tone_flag = 0xF0;

                    con_port = (dslot >> 5) + ((slot_params[slot].dmodule_ctone & 0xf) << 3);
                    con_slot = dslot & 0x1f;

                    if (con_port == TONE_E1 && con_slot == TONE_SILENT) {
                        ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_INACTIVE);
                        connect_tone(slot & 0x1f, slot >> 5, TONE_SILENT);
                    } else {
                        ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_ACTIVE);
                        connect_tone(slot & 0x1f, slot >> 5, con_slot); //???
                    }
                } else {
                    ds26518_e1_slot_enable(slot >> 5, slot & 0x1f, VOICE_INACTIVE);
                    connect_tone(slot & 0x1f, slot >> 5, TONE_SILENT);
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

    int changed = 0;
    u8_t ls_in[32];
    read_rx_abcd(e1_no, ls_in);

    for (int i = 1; i < MAX_E1_TIMESLOTS; i++) {
        if ((i != 16) && (slot_params[(e1_no << 5) + i].ls_in != ls_in[i])) {
            LOG_W("E1 '%d' slot '%d' ls_in %x <-- %x", 
                e1_no, i, ls_in[i], slot_params[(e1_no << 5) + i].ls_in);

            slot_params[(e1_no << 5) + i].ls_in = ls_in[i];
            changed = 1;
        }
    }

    if (changed) {
        //LOG_HEX("rev_abcd", 16, ls_in, 32);
        send_lsin_to_msc(e1_no);
    }
}

void mfc_scan(void)
{
    u8_t start_pos = TONE_E1 << 5;
    u8_t change_flag = 0;

    if (ram_params.mfc_module_installed == 0) {
        return;
    }
    
    for (u8_t i = 0; i < 32; i++) {
        slot_params[start_pos + i].old_mfc_par = read_dtmf(i);
        if (slot_params[start_pos + i].mfc_value != slot_params[start_pos + i].old_mfc_par) {

            LOG_D("slot '%d' receive new mfc code: %x -- > %x", i, slot_params[start_pos + i].mfc_value,
                slot_params[start_pos + i].old_mfc_par);

            slot_params[start_pos + i].mfc_value = slot_params[start_pos + i].old_mfc_par;
            change_flag = 1;
        }
    }

    if (change_flag) {
        send_mfc_par_to_msc(TONE_E1);
    }
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
            send_card_heartbeat(TO_OMC); //send to omc
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
            send_card_heartbeat(TO_OMC); // to omc
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

void update_e1_l1_status(void)
{
    static u8_t l1_st = 0;

   u8_t l1_status = 0;

    for (u8_t i = 0; i < E1_LINKS_MAX; i++) {
        l1_status |= (read_liu_status(i) << i);
        //ram_params.e1_l1_alarm &= ~(read_liu_status(i) << i)
    }

    ram_params.e1_l1_alarm = l1_status & (~(ram_params.conf_module_installed << 1));

    if (l1_st != l1_status) {
        LOG_W("card l1_lararm = %x, l1_status = %x", ram_params.e1_l1_alarm, l1_status);
        l1_st = l1_status;
    }

}

void update_e1_l2_status(void)
{
    static u8_t l2_st = 0;
   u8_t l2_status = 0;

    for (u8_t i = 0; i < E1_LINKS_MAX; i++) {
        l2_status |= (read_l2_status(i) << i);
    }

    ram_params.e1_l2_alarm = l2_status | (ram_params.conf_module_installed << 1);

    if (l2_st != l2_status) {
        LOG_W("card l2_lararm = %x, l2_status = %x", ram_params.e1_l2_alarm, l2_status);
        l2_st = l2_status;
    }
}

void period_50ms_proc(void *arg)
{
    (void)arg;

	static u8_t proc = 0;

    connect_tone_proc(0, 255);
    alarm_fsm(proc);
    proc = (proc + 1) & 0xF;

    sched_timeout(50, period_50ms_proc, NULL);
}

void period_20ms_proc(void *arg)
{
    (void)arg;

    for(int i = 0; i < E1_LINKS_MAX; i++) {
        ls_scan(i);
    }

    mfc_scan();

    sched_timeout(20, period_20ms_proc, NULL);
}

void period_500ms_proc(void *arg)
{
    static u32_t led_status = 0;

    (void)arg;

    led_status++;
    if (led_status & 1) {
        LED2_GREEN_ON;
    } else {
        LED2_OFF;
    }

    //update_e1_l1_status();
    read_e1_status();

    update_e1_l2_status();

    set_card_e1_led();

    sched_timeout(500, period_500ms_proc, NULL);
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
    memset(mtp2_heart_msg.led_ctl_code, 1, 8);
    memset(mtp2_heart_msg.dst_ip, 0xff, 8);
    memset(mtp2_heart_msg.retrieved_bsnt, 0x80, 8);
    mtp2_heart_msg.mtp2_soft_version = 0x50;
}

void set_mtp2_heart_msg_dstip(u8_t *dstip)
{
    //memcpy(mtp2_heart_msg.dst_ip, dstip, 8);
    for (int i = 0; i < 8; i++) {
        if (dstip[i] == 0xEE) {
            mtp2_heart_msg.dst_ip[i] = 0xFF;
        } else {
            mtp2_heart_msg.dst_ip[i] = dstip[i];
        }
    }
}

u8_t l2_alarm_to_state(u8_t e1_no)
{
    if ((1 << e1_no) & ram_params.e1_l2_alarm) {
        return MTP2_STATE_WORKING;
    } 
        
    return MTP2_STATE_ASSIGN;
}

void link_in_service(int e1_no)
{
    mtp2_heart_msg.timestamp = ram_params.timestamp;
    mtp2_heart_msg.led_ctl_code[e1_no & 7] = LED_IN_SERVICE;
    mtp2_heart_msg.alarm_component = e1_no & 7;

    mtp2_heart_msg.alarm_code = ALARM_IN_SERVICE;
    mtp2_heart_msg.l1_status = ~(ram_params.e1_l1_alarm);
    mtp2_heart_msg.l2_status[e1_no & 7] = l2_alarm_to_state(e1_no);
    mtp2_heart_msg.e1_port_type = e1_params.e1_port_type[card_id & 0x0f];
    mtp2_heart_msg.is_NT = e1_params.isdn_port_type[card_id & 0x0f];
    mtp2_heart_msg.mtp2_mode = e1_params.mtp2_error_check[card_id & 0x0f];

    LOG_I("Link '%d' is in service:", e1_no);
    LOG_D("link '%d' l1_status = %x", e1_no, mtp2_heart_msg.l1_status);
    LOG_D("link '%d' l2_status=%d", e1_no, l2_alarm_to_state(e1_no));
    //LOG_D("e1_port type = %X", e1_params.e1_port_type[card_id & 0x0f]);
    //LOG_D("nt type = %X", e1_params.isdn_port_type[card_id & 0x0f]);
    send_mtp2_trap_msg();    
}

void link_outof_service(int e1_no, u8_t alarm_code)
{
    mtp2_heart_msg.timestamp = ram_params.timestamp;
    mtp2_heart_msg.led_ctl_code[e1_no & 7] = LED_ALIGNMENT;
    mtp2_heart_msg.alarm_component = e1_no & 7;

    mtp2_heart_msg.l1_status = ~ram_params.e1_l1_alarm;
    mtp2_heart_msg.l2_status[e1_no & 7] = l2_alarm_to_state(e1_no);
    //mtp2_heart_msg.retrieved_bsnt =  0;

    send_mtp2_trap_msg();
}

static void update_heartbeat_msg(void)
{
    hb_msg.timestamp = ram_params.timestamp;
    mtp2_heart_msg.timestamp = ram_params.timestamp;

    for (int i = 0; i < E1_LINKS_MAX; i++) {
        if (ram_params.e1_l2_alarm & (1 << i)) {
            mtp2_heart_msg.led_ctl_code[i] = LED_IN_SERVICE;
            mtp2_heart_msg.l2_status[i] = MTP2_STATE_WORKING;
            hb_msg.omcled[i] = LED_IN_SERVICE;
        } else {
            mtp2_heart_msg.led_ctl_code[i] = LED_ALIGNMENT;
            mtp2_heart_msg.l2_status[i] = MTP2_STATE_ASSIGN;
            hb_msg.omcled[i] = LED_ALIGNMENT;
        }
        
    }

    mtp2_heart_msg.alarm_code = ALARM_IN_SERVICE;
    mtp2_heart_msg.l1_status = ~(ram_params.e1_l1_alarm);
    mtp2_heart_msg.e1_port_type = e1_params.e1_port_type[card_id & 0x0f];
    mtp2_heart_msg.is_NT = e1_params.isdn_port_type[card_id & 0x0f];
    mtp2_heart_msg.mtp2_mode = e1_params.mtp2_error_check[card_id & 0x0f];
}

void period_10s_proc(void *arg)
{
    static u8_t pingpang = TO_OMC;

    (void)arg;

    update_heartbeat_msg();
    
    if (pingpang == TO_OMC) {
        send_card_heartbeat(pingpang);
        pingpang = plat_no;
    } else {
        send_card_heartbeat(pingpang);
        pingpang = TO_OMC;
    }

    send_mtp2_trap_msg();

    //ds26518_frame_status(0);

    sched_timeout(10000, period_10s_proc, NULL);
}

void start_period_proc(void)
{
    period_50ms_proc(NULL);

    period_20ms_proc(NULL);

    period_500ms_proc(NULL);

    //period_10s_proc(NULL);
}

void update_e1_enable(u8_t new_value)
{
    u8_t mask;
    u8_t old_value = e1_params.e1_enable[card_id];

    for(int i = 0; i < E1_LINKS_MAX; i++) {
        mask = 1 << i;

        if (!(old_value & mask) && (new_value & mask)) {
            e1_port_init(i);
        } else if ((old_value & mask) && !(new_value & mask)) {
            e1_port_init(i);
        }
    }

    e1_params.e1_enable[card_id] = new_value;
}

void init_r2_param(void)
{
    for (int i = 0; i < 256; i++) {
        slot_params[i].chk_times = 0;
        slot_params[i].ls_in = slot_params[i].ls_out = 0;
        slot_params[i].old_mfc_par = 0;
        slot_params[i].mfc_value = 0;
    }
}

void init_mfc_slot(void)
{
    for(int i = 0; i < MAX_E1_TIMESLOTS; i++) {
        connect_slot(i, TONE_E1, 0, TONE_E1);
    }
}
