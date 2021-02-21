#include <stdarg.h>
#include <string.h>
#include "main.h"
#include "ulog.h"
#include "rt_service.h"

#define ULOG_LINE_BUF_SIZE 128

/**
 * CSI(Control Sequence Introducer/Initiator) sign
 * more information on https://en.wikipedia.org/wiki/ANSI_escape_code
 */
#define CSI_START "\033["
#define CSI_END "\033[0m"
/* output log front color */
#define F_BLACK "30m"
#define F_RED "31m"
#define F_GREEN "32m"
#define F_YELLOW "33m"
#define F_BLUE "34m"
#define F_MAGENTA "35m"
#define F_CYAN "36m"
#define F_WHITE "37m"

/* output log default color definition */
#ifndef ULOG_COLOR_DEBUG
#define ULOG_COLOR_DEBUG NULL
#endif
#ifndef ULOG_COLOR_INFO
#define ULOG_COLOR_INFO (F_BLUE)
#endif
#ifndef ULOG_COLOR_WARN
#define ULOG_COLOR_WARN (F_YELLOW)
#endif
#ifndef ULOG_COLOR_ERROR
#define ULOG_COLOR_ERROR (F_RED)
#endif
#ifndef ULOG_COLOR_ASSERT
#define ULOG_COLOR_ASSERT (F_MAGENTA)
#endif

struct rt_ulog
{
    int init_ok;
    sys_mutex_t output_locker;
    /* all backends */
    rt_slist_t backend_list;
    /* the thread log's line buffer */
    char log_buf_th[ULOG_LINE_BUF_SIZE];
};


/* level output info */
/**
static const char *const level_output_info[] =
    {
        "A/",
        NULL,
        NULL,
        "E/",
        "W/",
        NULL,
        "I/",
        "D/",
};
**/

/* color output info */
static const char *const color_output_info[] =
    {
        ULOG_COLOR_ASSERT,
        NULL,
        NULL,
        ULOG_COLOR_ERROR,
        ULOG_COLOR_WARN,
        NULL,
        ULOG_COLOR_INFO,
        ULOG_COLOR_DEBUG,
};

/* ulog local object */
static struct rt_ulog ulog = {0};

size_t ulog_strcpy(size_t cur_len, char *dst, const char *src)
{
    const char *src_old = src;

    ASSERT(dst);
    ASSERT(src);

    while (*src != 0)
    {
        /* make sure destination has enough space */
        if (cur_len++ < ULOG_LINE_BUF_SIZE)
        {
            *dst++ = *src++;
        }
        else
        {
            break;
        }
    }
    return src - src_old;
}

size_t ulog_ultoa(char *s, unsigned long int n)
{
    size_t i = 0, j = 0, len = 0;
    char swap;

    do
    {
        s[len++] = n % 10 + '0';
    } while (n /= 10);
    s[len] = '\0';
    /* reverse string */
    for (i = 0, j = len - 1; i < j; ++i, --j)
    {
        swap = s[i];
        s[i] = s[j];
        s[j] = swap;
    }
    return len;
}

static void output_unlock(void)
{
    sys_mutex_unlock(&ulog.output_locker);
}

static void output_lock(void)
{
    sys_mutex_lock(&ulog.output_locker);
}

static char *get_log_buf(void)
{
    return ulog.log_buf_th;
}

__WEAK size_t ulog_formater(char *log_buf, u32_t level, const char *tag, int newline,
                            const char *format, va_list args)
{
    /* the caller has locker, so it can use static variable for reduce stack usage */
    static size_t log_len, newline_len;
    static int fmt_result;

    LWIP_ASSERT("log_buf != NULL", log_buf);
    LWIP_ASSERT("level <= LOG_LVL_DBG", level <= LOG_LVL_DBG);
    LWIP_ASSERT("tag != NULL", tag);
    LWIP_ASSERT("format != NULL", format);

    log_len = 0;
    newline_len = strlen(ULOG_NEWLINE_SIGN);

    /* add CSI start sign and color info */
    if (color_output_info[level])
    {
        log_len += ulog_strcpy(log_len, log_buf + log_len, CSI_START);
        log_len += ulog_strcpy(log_len, log_buf + log_len, color_output_info[level]);
    }

    /* time stamp */
    static size_t tick_len = 0;
    log_buf[log_len] = '[';
    tick_len = ulog_ultoa(log_buf + log_len + 1, HAL_GetTick());
    log_buf[log_len + 1 + tick_len] = ']';
    log_buf[log_len + 1 + tick_len + 1] = '\0';
    log_len += strlen(log_buf + log_len);

    /* tag */
    log_len += ulog_strcpy(log_len, log_buf + log_len, " ");
    log_len += ulog_strcpy(log_len, log_buf + log_len, tag);

    log_len += ulog_strcpy(log_len, log_buf + log_len, ": ");

    fmt_result = vsnprintf(log_buf + log_len, ULOG_LINE_BUF_SIZE - log_len, format, args);

    /* calculate log length */
    if ((log_len + fmt_result <= ULOG_LINE_BUF_SIZE) && (fmt_result > -1))
    {
        log_len += fmt_result;
    }
    else
    {
        /* using max length */
        log_len = ULOG_LINE_BUF_SIZE;
    }

    /* overflow check and reserve some space for CSI end sign and newline sign */

    if (log_len + (sizeof(CSI_END) - 1) + newline_len > ULOG_LINE_BUF_SIZE)
    {
        /* using max length */
        log_len = ULOG_LINE_BUF_SIZE;
        /* reserve some space for CSI end sign */
        log_len -= (sizeof(CSI_END) - 1);
        /* reserve some space for newline sign */
        log_len -= newline_len;
    }

    /* package newline sign */
    if (newline)
    {
        log_len += ulog_strcpy(log_len, log_buf + log_len, ULOG_NEWLINE_SIGN);
    }

    /* add CSI end sign  */
    if (color_output_info[level])
    {
        log_len += ulog_strcpy(log_len, log_buf + log_len, CSI_END);
    }

    return log_len;
}

void ulog_output_to_all_backend(const char *log, size_t size)
{
    rt_slist_t *node;
    ulog_backend_t backend;

    if (!ulog.init_ok)
        return;

    /* output for all backends */
    for (node = rt_slist_first(&ulog.backend_list); node; node = rt_slist_next(node))
    {
        backend = rt_slist_entry(node, struct ulog_backend, list);
        backend->output(backend, log, size);
    }
}

static void do_output(const char *log_buf, size_t log_len)
{
   ulog_output_to_all_backend(log_buf, log_len);
}

/**
 * output the log by variable argument list
 *
 * @param level level
 * @param tag tag
 * @param newline has_newline
 * @param format output format
 * @param args variable argument list
 */
void ulog_voutput(u32_t level, const char *tag, int newline, const char *format, va_list args)
{
    char *log_buf = NULL;
    size_t log_len = 0;

    LWIP_ASSERT("level <= LOG_LVL_DBG", level <= LOG_LVL_DBG);
    LWIP_ASSERT("tag != NULL", tag);
    LWIP_ASSERT("format != NULL", format);

    if (!ulog.init_ok)
    {
        return;
    }

    /* get log buffer */
    log_buf = get_log_buf();

    /* lock output */
    output_lock();

    log_len = ulog_formater(log_buf, level, tag, newline, format, args);

    do_output(log_buf, log_len);
    /* unlock output */
    output_unlock();
}

/**
 * output the log
 *
 * @param level level
 * @param tag tag
 * @param newline has newline
 * @param format output format
 * @param ... args
 */
void ulog_output(u32_t level, const char *tag, int newline, const char *format, ...)
{
    va_list args;

    /* args point to the first variable parameter */
    va_start(args, format);

    ulog_voutput(level, tag, newline, format, args);

    va_end(args);
}

/**
 * output RAW string format log
 *
 * @param format output format
 * @param ... args
 */
void ulog_raw(const char *format, ...)
{
    size_t log_len = 0;
    char *log_buf = NULL;
    va_list args;
    int fmt_result;

    LWIP_ASSERT("ulog.init_ok != 0", ulog.init_ok);

    /* get log buffer */
    log_buf = get_log_buf();

    /* lock output */
    output_lock();
    /* args point to the first variable parameter */
    va_start(args, format);

    fmt_result = vsnprintf(log_buf, ULOG_LINE_BUF_SIZE, format, args);

    va_end(args);

    /* calculate log length */
    if ((fmt_result > -1) && (fmt_result <= ULOG_LINE_BUF_SIZE))
    {
        log_len = fmt_result;
    }
    else
    {
        log_len = ULOG_LINE_BUF_SIZE;
    }

    /* do log output */
    do_output(log_buf, log_len);

    /* unlock output */
    output_unlock();
}

/**
 * dump the hex format data to log
 *
 * @param tag name for hex object, it will show on log header
 * @param width hex number for every line, such as: 16, 32
 * @param buf hex buffer
 * @param size buffer size
 */
void ulog_hexdump(const char *tag, size_t width, u8_t *buf, size_t size)
{
#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')

    size_t i, j;
    size_t log_len = 0, name_len = strlen(tag);
    char *log_buf = NULL, dump_string[8];
    int fmt_result;

    LWIP_ASSERT("ulog.init_ok != 0", ulog.init_ok);

    /* get log buffer */
    log_buf = get_log_buf();

    /* lock output */
    output_lock();

    for (i = 0, log_len = 0; i < size; i += width)
    {
        /* package header */
        if (i == 0)
        {
            log_len += ulog_strcpy(log_len, log_buf + log_len, "D/HEX ");
            log_len += ulog_strcpy(log_len, log_buf + log_len, tag);
            log_len += ulog_strcpy(log_len, log_buf + log_len, ": ");
        }
        else
        {
            log_len = 6 + name_len + 2;
            memset(log_buf, ' ', log_len);
        }
        fmt_result = snprintf(log_buf + log_len, ULOG_LINE_BUF_SIZE, "%04X-%04X: ", i, i + width - 1);
        /* calculate log length */
        if ((fmt_result > -1) && (fmt_result <= ULOG_LINE_BUF_SIZE))
        {
            log_len += fmt_result;
        }
        else
        {
            log_len = ULOG_LINE_BUF_SIZE;
        }
        /* dump hex */
        for (j = 0; j < width; j++)
        {
            if (i + j < size)
            {
                snprintf(dump_string, sizeof(dump_string), "%02X ", buf[i + j]);
            }
            else
            {
                strncpy(dump_string, "   ", sizeof(dump_string));
            }
            log_len += ulog_strcpy(log_len, log_buf + log_len, dump_string);
            if ((j + 1) % 8 == 0)
            {
                log_len += ulog_strcpy(log_len, log_buf + log_len, " ");
            }
        }
        log_len += ulog_strcpy(log_len, log_buf + log_len, "  ");
        /* dump char for hex */
        for (j = 0; j < width; j++)
        {
            if (i + j < size)
            {
                snprintf(dump_string, sizeof(dump_string), "%c", __is_print(buf[i + j]) ? buf[i + j] : '.');
                log_len += ulog_strcpy(log_len, log_buf + log_len, dump_string);
            }
        }
        /* overflow check and reserve some space for newline sign */
        if (log_len + strlen(ULOG_NEWLINE_SIGN) > ULOG_LINE_BUF_SIZE)
        {
            log_len = ULOG_LINE_BUF_SIZE - strlen(ULOG_NEWLINE_SIGN);
        }
        /* package newline sign */
        log_len += ulog_strcpy(log_len, log_buf + log_len, ULOG_NEWLINE_SIGN);
        /* do log output */
        do_output(log_buf, log_len);
    }
    /* unlock output */
    output_unlock();
}

int ulog_backend_register(ulog_backend_t backend, const char *name, int support_color)
{
    LWIP_ASSERT("backend != NULL", backend);
    LWIP_ASSERT("name != NULL", name);
    LWIP_ASSERT("ulog.init_ok != 0", ulog.init_ok);
    LWIP_ASSERT("backend->output != NULL", backend->output);

    if (backend->init)
    {
        backend->init(backend);
    }

    backend->support_color = support_color;
    memcpy(backend->name, name, BACKLEND_NAME_MAX);

    taskENTER_CRITICAL();
    rt_slist_append(&ulog.backend_list, &backend->list);
    taskEXIT_CRITICAL();

    return 0;
}

int ulog_backend_unregister(ulog_backend_t backend)
{
    LWIP_ASSERT("backend != NULL", backend);
    LWIP_ASSERT("ulog.init_ok != 0", ulog.init_ok);

    if (backend->deinit)
    {
        backend->deinit(backend);
    }

    taskENTER_CRITICAL();
    rt_slist_remove(&ulog.backend_list, &backend->list);
    taskEXIT_CRITICAL();

    return 0;
}

/**
 * flush all backends's log
 */
void ulog_flush(void)
{
    rt_slist_t *node;
    ulog_backend_t backend;

    if (!ulog.init_ok)
        return;

    /* flush all backends */
    for (node = rt_slist_first(&ulog.backend_list); node; node = rt_slist_next(node))
    {
        backend = rt_slist_entry(node, struct ulog_backend, list);
        if (backend->flush)
        {
            backend->flush(backend);
        }
    }
}

int ulog_init(void)
{
    if (ulog.init_ok)
        return 0;

    if (sys_mutex_new(&ulog.output_locker) != ERR_OK)
    {
        LWIP_ASSERT("failed to create ulog.output_locker", 0);
    }

    rt_slist_init(&ulog.backend_list);

    ulog.init_ok = 1;

    return 0;
}

void ulog_deinit(void)
{
    rt_slist_t *node;
    ulog_backend_t backend;

    if (!ulog.init_ok)
        return;

    /* deinit all backends */
    for (node = rt_slist_first(&ulog.backend_list); node; node = rt_slist_next(node))
    {
        backend = rt_slist_entry(node, struct ulog_backend, list);
        if (backend->deinit)
        {
            backend->deinit(backend);
        }
    }

    sys_mutex_free(&ulog.output_locker);

    ulog.init_ok = 0;
}
