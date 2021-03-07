
#include "lwip/opt.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "lwip/mem.h"
#include "lwip/sys.h"
#include "lwip/def.h"
#include "lwip/api.h"
#include "shell_port.h"
#include "lffifo.h"
#include "ulog.h"

Shell shell;
char shellBuffer[512];


#define BUFSIZE     1024

static struct lffifo *telnet_rev_buf = NULL;
static struct netconn *telnet_conn = NULL;

void telnetShellWrite(char data)
{
    if (telnet_conn != NULL) {
        netconn_write(telnet_conn, (const void *)&data, 1, NETCONN_NOCOPY);
    }
}


signed char telnetShellRead(char *data)
{
    static u8_t rev_buf[128] = {0};
    static u8_t frame_size = 0;
    static u8_t get_index = 0;

start:
    if (frame_size == 0 && get_index == 0) {
        frame_size = lffifo_get(telnet_rev_buf, rev_buf, 128);       
    }
    
    if (frame_size > 0) {
        if (get_index < frame_size) {
            *data = rev_buf[get_index];
            get_index++;
            return 0;
        } else {
            frame_size = get_index = 0;
            goto start;
        }
    } 
    
    return -1;
}

void userShellInit(void)
{
    shell.write = telnetShellWrite;
    shell.read = telnetShellRead;
    shellInit(&shell, shellBuffer, 512);
}

static void shell_main(struct netconn *conn)
{
    struct pbuf *p;
    err_t ret;

    do {
        ret = netconn_recv_tcp_pbuf(conn, &p);
        if (ret == ERR_OK) {
            lffifo_put(telnet_rev_buf, (u8_t *)p->payload, p->tot_len);
            netconn_write(conn, (const void *)p->payload, p->tot_len, NETCONN_NOCOPY);
            pbuf_free(p);
        }

    } while (ret == ERR_OK);
    printf("err %s\n", lwip_strerr(ret));

    netconn_close(conn);
    netconn_delete(conn);
    conn = NULL;
}

static struct ulog_backend telnet_console = {0};

static void telnet_backend_output(struct ulog_backend *backend, const char *log_buf, size_t log_len)
{
  if (telnet_conn != NULL) {
     netconn_write(telnet_conn, (const void *)log_buf, log_len, NETCONN_NOCOPY);
  }
}

static int telnet_backend_init(void)
{
  ulog_init();
  telnet_console.output = telnet_backend_output;

  ulog_backend_register(&telnet_console, "telnet", 1);

  return 0;
}

static void shell_thread(void *arg)
{
    struct netconn *conn;
    //struct netconn *new_conn;
    err_t err;
    LWIP_UNUSED_ARG(arg);

    conn = netconn_new(NETCONN_TCP);
    LWIP_ERROR("shell: invalid conn", (conn != NULL), return;);
    err = netconn_bind(conn, IP_ADDR_ANY, 23);

    LWIP_ERROR("shell: netconn_bind failed", (err == ERR_OK), netconn_delete(conn); return;);
    err = netconn_listen(conn);
    LWIP_ERROR("shell: netconn_listen failed", (err == ERR_OK), netconn_delete(conn); return;);

    telnet_rev_buf = lffifo_alloc(BUFSIZE);
    LWIP_ERROR("shell: telnet rev buf failed", (telnet_rev_buf != NULL), netconn_delete(conn); return;);

    for (;;) {
        err = netconn_accept(conn, &telnet_conn);
        if (err == ERR_OK) {
            telnet_backend_init();
            userShellInit();
            shell_main(telnet_conn);
            
            ulog_backend_unregister(&telnet_console);
            netconn_delete(telnet_conn);
        }
    }
}

void shell_init(void)
{
    sys_thread_new("shell_thread", shell_thread, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
}
