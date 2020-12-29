
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lwip/sys.h"
#include "lwip.h"
#include "lwip/api.h"

#include "main.h"
#include "mtp.h"
#include "eeprom.h"
#include "8ke1_debug.h"
#include "server_interface.h"

static struct netconn *ss7_conn;
static struct netconn *isdn_conn;
//static struct netconn *other_conn;

static void other_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr)
{
    struct other_msg  *ot_msg = (struct other_msg *)p->payload;
    
    switch (ot_msg->m_head.msg_type) {
        case CON_TIME_SLOT:
            
            break;
        case CON_TONE:
            break;
        case CON_DTMF:
            break;
        case CON_CONN_GRP:
            break;
        case CON_DISC_GRP:
            break;
        case CON_DEC_DTMF:
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

    struct isdn_msg *rev_msg = (struct isdn_msg *)p->payload;
    
    uint8_t pd = rev_msg->msg.l3msg.pd;

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
    netconn_bind(conn, IP4_ADDR_ANY, ISDN_UDP_PORT);

    CARD_ERROR("isdn_netconn: invalid conn", (conn != NULL), return;);
    isdn_conn = conn;

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
  netconn_bind(conn, IP4_ADDR_ANY, SS7_UDP_PORT);

  CARD_ERROR("ss7_netconn: invalid conn", (conn != NULL), return;);
  ss7_conn = conn;

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

void send_ss7_msg(u8_t link_no, u8_t *buf, u8_t len)
{
    ip4_addr_t  local_addr;
    u16_t  local_port;
    const ip4_addr_t *dst_addr;

    struct netbuf *net_buf = netbuf_new();
    u16_t tot_len = sizeof(struct ss7_msg) + len + 3;

    struct ss7_msg *sig_msg = (struct ss7_msg *)netbuf_alloc(net_buf, tot_len);
    if (sig_msg == NULL) {
        CARD_DEBUGF(MSG_DEBUG, ("failed to alloc mem for ss7_msg\n"));
        return;
    }
    
    netconn_getaddr(ss7_conn, &local_addr, &local_port, 1);
    //netconn_getaddr(ss7_conn, &remote_addr, &remote_port, 0);

    if (plat_no) {
        sig_msg->ip_head.msgDstIp = sn1.addr;
        dst_addr = &sn1;
    } else {
        sig_msg->ip_head.msgDstIp = sn0.addr;
        dst_addr = &sn0;
    }
    sig_msg->ip_head.msgSrcIp = local_addr.addr;
    sig_msg->ip_head.msgSrcPort = SS7_UDP_PORT;
    //sig_msg->ip_head.msgDstIp = remote_addr.addr;
    sig_msg->ip_head.msgDstPort = SS7_UDP_PORT;
    sig_msg->ip_head.msgBroadcast = 0;
    sig_msg->ip_head.msgLens = len + 2;

    sig_msg->msg.e1_no = link_no + (card_id << 3);
    sig_msg->msg.len = len;
    sig_msg->msg.sio = buf[0];
    memcpy(sig_msg->msg.msg, &buf[1], len);

    if (netconn_sendto(ss7_conn, net_buf, dst_addr, SS7_UDP_PORT) != ERR_OK) {
        CARD_DEBUGF(MSG_DEBUG, ("send ss7 msg failed.\n"));
    }

    netbuf_delete(net_buf);
}

/* len = msg->m_head.msu_len;
*/
void send_other_msg(struct other_msg *msg, u8_t len)
{
    ip4_addr_t local_addr;
    u16_t local_port;
    const ip4_addr_t *dst_addr;

    struct netbuf *net_buf = netbuf_new();
    u16_t tot_len = sizeof(struct ip_head) + len;

    struct other_msg *send_msg = (struct other_msg *)netbuf_alloc(net_buf, tot_len);
    if (send_msg == NULL)
    {
        CARD_DEBUGF(MSG_DEBUG, ("failed to alloc mem for other_msg\n"));
        return;
    }

    netconn_getaddr(ss7_conn, &local_addr, &local_port, 1);

    send_msg->ip_head.msgSrcIp = local_addr.addr;
    send_msg->ip_head.msgSrcPort = OTHER_UDP_PORT;
    if (plat_no) {
        send_msg->ip_head.msgDstIp = sn1.addr;
        dst_addr = &sn1;
    } else {
        send_msg->ip_head.msgDstIp = sn0.addr;
        dst_addr = &sn0;
    }
    send_msg->ip_head.msgDstPort = OTHER_UDP_PORT;
    send_msg->ip_head.msgBroadcast = 0;
    send_msg->ip_head.msgLens = msg->m_head.msu_len + 1;

    memcpy((void *)&send_msg->m_head.msu_len, (void *)&msg->m_head.msu_len, len+1);

    if (netconn_sendto(ss7_conn, net_buf, dst_addr, OTHER_UDP_PORT) != ERR_OK) {
        CARD_DEBUGF(MSG_DEBUG, ("send other msg failed.\n"));
    }

    netbuf_delete(net_buf);
}