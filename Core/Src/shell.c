
#include "lwip/opt.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "lwip/mem.h"
#include "lwip/sys.h"
#include "lwip/def.h"
#include "lwip/api.h"
#include "shell.h"

#define NEWLINE     "\n"

#define BUFSIZE     1024

static unsigned char buffer[BUFSIZE];

struct command {
    struct netconn *conn;
    s8_t (* exec)(struct command *);
    u8_t nargs;
    char *args[10];
};

#define ESUCCESS    0
#define ESYNTAX     -1
#define ETOOFEW     -2
#define ETOOMANY    -3
#define ECLOSED     -4

static struct netconn *telnet_conn;

static char help_msg[] = "Available commands:"NEWLINE"\
quit: quits"NEWLINE"";

void sendstr(const char *str)
{
    if (telnet_conn != NULL) {
        netconn_write(telnet_conn, (const void *)str, strlen(str), NETCONN_NOCOPY);
    }
}

void eth_printf(const char *fmt, ...)
{
    char buf[512];

    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    
    sendstr(buf);
}

static void shell_error(s8_t err, struct netconn *conn)
{
    (void)conn; 
    switch (err) {
        case ESYNTAX:
            sendstr("## Syntax error"NEWLINE);
            break;
        case ETOOFEW:
            sendstr("## Too few arguments to command given"NEWLINE);
            break;
        case ETOOMANY:
            sendstr("## Too many arguments to command given"NEWLINE);
            break;
        case ECLOSED:
            sendstr("## Connection closed"NEWLINE);
            break;
        default:
            break;
    }
}

static s8_t com_help(struct command *com)
{
    sendstr(help_msg);
    return ESUCCESS;
}

static s8_t parse_command(struct command *com, u32_t len)
{
    u16_t i;
    u16_t bufp;

    if (strncmp((const char *)buffer, "help", 4) == 0) {
        com->exec = com_help;
        com->nargs = 0;
    } else if (strncmp((const char *)buffer, "quit", 4) == 0) {
        printf("quit"NEWLINE);
        return ECLOSED;
    } else {
        return ESYNTAX;
    }

    if (com->nargs == 0) {
        return ESUCCESS;
    }

    bufp = 0;
    for (; bufp < len && buffer[bufp] != ' '; bufp++);
    for (i = 0; i < 10; i++) {
        for (; bufp < len && buffer[bufp] == ' '; bufp++);
        if (buffer[bufp] == '\r' || buffer[bufp] == '\n') {
            buffer[bufp] = 0;
            if ( i < com->nargs - 1) {
                return ETOOFEW;
            }
            if (i > com->nargs - 1) {
                return ETOOMANY;
            }
            break;
        }

        if (bufp > len) {
            return ETOOFEW;
        }

        com->args[i] = (char *)&buffer[bufp];
        for (; bufp < len && buffer[bufp] != ' ' && buffer[bufp] != '\r' &&
        buffer[bufp] != '\n'; bufp++) {
            if (buffer[bufp] == '\\') {
                buffer[bufp] = ' ';
            }
        }

        if (bufp > len) {
            return ESYNTAX;
        }

        buffer[bufp] = 0;
        bufp++;
        if (i == com->nargs - 1) {
            break;
        }
    }
    return ESUCCESS;
}

static void prompt(struct netconn *conn)
{
    sendstr("> ");
}

static void shell_main(struct netconn *conn)
{
    struct pbuf *p;
    u16_t len = 0, cur_len;
    struct command com;
    s8_t err;

    err_t ret;

    //void *echomem;

    do {
        ret = netconn_recv_tcp_pbuf(conn, &p);
        if (ret == ERR_OK) {
            pbuf_copy_partial(p, &buffer[len], (u16_t)(BUFSIZE - len), 0);
            cur_len = p->tot_len;
            len = (u16_t)(len + cur_len);
            if ((len < cur_len) || (len > BUFSIZE)) {
                len = BUFSIZE;
            }
 /*       
            echomem = mem_malloc(cur_len);
            if (echomem != NULL) {
                pbuf_copy_partial(p, echomem, cur_len, 0);
                netconn_write(conn, echomem, cur_len, NETCONN_COPY);
                mem_free(echomem);
            }
*/
            pbuf_free(p);
            if (((len > 0) && ((buffer[len-1] == '\r') || (buffer[len-1] == '\n'))) ||
                (len >= BUFSIZE)) {
                if (buffer[0] != 0xff && buffer[1] != 0xfe) {
                    err = parse_command(&com, len);
                    if (err == ESUCCESS) {
                        com.conn = conn;
                        err = com.exec(&com);
                    }
                    if (err == ECLOSED) {
                        printf("Closed"NEWLINE);
                        shell_error(err, conn);
                        goto close;
                    }
                    if (err != ESUCCESS) {
                        shell_error(err, conn);
                    }
                } else {
                    sendstr(NEWLINE NEWLINE
                            "8KE1 simple interactive shell."NEWLINE
                            "For help, try the \"help\" command."NEWLINE);
                }
                if (ret == ERR_OK) {
                    prompt(conn);
                }
                len = 0;
            }
        }

    } while (ret == ERR_OK);
    printf("err %s"NEWLINE, lwip_strerr(ret));

close:
    netconn_close(conn);
    netconn_delete(conn);
    conn = NULL;
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

    for (;;) {
        err = netconn_accept(conn, &telnet_conn);
        if (err == ERR_OK) {
            shell_main(telnet_conn);
            netconn_delete(telnet_conn);
        }
    }
}

void shell_init(void)
{
    sys_thread_new("shell_thread", shell_thread, NULL, DEFAULT_THREAD_STACKSIZE, DEFAULT_THREAD_PRIO);
}
