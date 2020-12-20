
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "8ke1_debug.h"
#include "server_interface.h"
#include "lwip/sys.h"
#include "lwip.h"
#include "lwip/api.h"

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

    uint8_t sio = sig_msg->msg.ss7Msg.sio;

    if (sio < 7) {
        //Send msg to mtp thead by mailbox?
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