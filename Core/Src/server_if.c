
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
static struct netconn *other_conn;

static void other_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr, u16_t port)
{
    LWIP_UNUSED_ARG(conn);

    udpMsg_t *rev_msg = (udpMsg_t *)p->payload;
    commandMsg_t *comm_msg = (commandMsg_t *)rev_msg->msgContents;

    switch (comm_msg->msgType) {
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

static void isdn_receive(struct netconn *conn, struct pbuf *p, const ip_addr_t *src_addr, u16_t port)
{
    LWIP_UNUSED_ARG(conn);

    udpMsg_t *rev_msg = (udpMsg_t *)p->payload;
    signalMsg_t *sig_msg = (signalMsg_t *)rev_msg->msgContents;

    uint8_t pd = sig_msg->msg.isdnMsg.pd;

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
    LWIP_UNUSED_ARG(conn);

    udpMsg_t *rev_msg = (udpMsg_t *)p->payload;
    signalMsg_t *sig_msg = (signalMsg_t *)rev_msg->msgContents;

    u8_t sio = sig_msg->msg.ss7Msg.sio;

    if (sio < 7) {
        mtp2_t *m = get_mtp2_state(sig_msg->linkNo);
        mtp2_queue_msu(m, sio, sig_msg->msg.ss7Msg.msgContents, (int)sig_msg->msgLens);
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
    sys_thread_new("other_netconn", other_netconn_thread, NULL, SS7_STACK_SIZE, osPriorityNormal);
}

void send_ss7_msg(u8_t link_no, u8_t *buf, u8_t len)
{
    ip4_addr_t  local_addr, remote_addr;
    u16_t  local_port, remote_port;

    struct netbuf *net_buf = netbuf_new();
    u16_t tot_len = sizeof(udpMsg_t) + len + 3;

    udpMsg_t *udp_msg = (udpMsg_t *)netbuf_alloc(net_buf, tot_len);
    if (udp_msg == NULL) {
        CARD_DEBUGF(MSG_DEBUG, ("failed to alloc mem for updmsg\n"));
        return;
    }
    
    netconn_getaddr(ss7_conn, &local_addr, &local_port, 1);
    netconn_getaddr(ss7_conn, &remote_addr, &remote_port, 0);

    udp_msg->msgSrcIp = local_addr.addr;
    udp_msg->msgSrcPort = SS7_UDP_PORT;
    udp_msg->msgDstIp = remote_addr.addr;
    udp_msg->msgDstPort = SS7_UDP_PORT;
    udp_msg->msgBroadcast = 0;
    udp_msg->msgLens = len + 2;

    signalMsg_t *sig_msg = (signalMsg_t *)udp_msg->msgContents;
    sig_msg->linkNo = link_no;
    sig_msg->msgLens = len;
    sig_msg->msg.ss7Msg.sio = buf[3] & 0xF;
    memcpy(sig_msg->msg.ss7Msg.msgContents, buf, len);

    if (netconn_send(ss7_conn, net_buf) != ERR_OK) {
        CARD_DEBUGF(MSG_DEBUG, ("send udp msg failed.\n"));
    }

    netbuf_delete(net_buf);
}
