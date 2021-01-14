#include "main.h"
#include "sched.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "card_debug.h"

#define SCHED_MAX_TIMEOUT   0x7fffffff
#define SCHED_STACK_SIZE      128*4

#define TIME_LESS_THAN(t, compare_to)   ( (((u32_t)((t)-(compare_to))) > SCHED_MAX_TIMEOUT) ? 1 : 0)

static struct sched_timeo *next_timeout;

static u32_t current_timeout_due_time;

static void sched_timeout_abs(u32_t abs_time, sched_timeout_handler handler, void *arg)
{
    struct sched_timeo *timeout, *t;

    timeout = (struct sched_timeo *)pvPortMalloc(sizeof(struct sched_timeo));

    if (timeout == NULL) {
        CARD_ASSERT("sched_timeout: timeout != NULL, FreeRtos memory pool is empty", timeout != NULL);
        return;
    }

    timeout->next = NULL;
    timeout->h = handler;
    timeout->arg = arg;
    timeout->time = abs_time;

    if (next_timeout == NULL) {
        next_timeout = timeout;
        return;
    }

    if (TIME_LESS_THAN(timeout->time, next_timeout->time)) {
        timeout->next = next_timeout;
        next_timeout = timeout;
    } else {
        for (t = next_timeout; t != NULL; t = t->next) {
            if ((t->next == NULL) || TIME_LESS_THAN(timeout->time, t->next->time)) {
                timeout->next = t->next;
                t->next = timeout;
                break;
            }
        }
    }
}

void sched_timeout(u32_t msecs, sched_timeout_handler handler, void *arg)
{
    u32_t next_timeout_time;

    next_timeout_time = (u32_t)(HAL_GetTick() + msecs); /* overflow handled by TIME_LESS_THAN macro */

    sched_timeout_abs(next_timeout_time, handler, arg);
}

void sched_untimeout(sched_timeout_handler handler, void *arg)
{
    struct sched_timeo *prev_t, *t;

    if (next_timeout == NULL) {
        return;
    }

    for (t = next_timeout, prev_t = NULL; t != NULL; prev_t = t, t = t->next) {
        if ((t->h == handler) && (t->arg == arg)) {
            if (prev_t == NULL) {
                next_timeout = t->next;
            } else {
                prev_t->next = t->next;
            }
            vPortFree(t);
            return;
        }
    }
}

void sched_check_timeouts(void)
{
    u32_t now;

    now = HAL_GetTick();

    do {
        struct sched_timeo *tmptimeout;
        sched_timeout_handler handler;
        void *arg;

        tmptimeout = next_timeout;
        if (tmptimeout == NULL) {
            return;
        }

        if (TIME_LESS_THAN(now, tmptimeout->time)) {
            return;
        }

        next_timeout = tmptimeout->next;
        handler = tmptimeout->h;
        arg = tmptimeout->arg;
        current_timeout_due_time = tmptimeout->time;
        vPortFree(tmptimeout);
        
        if (handler != NULL) {
            handler(arg);
        }

    } while (1);
}

void sched_restart_timeouts(void)
{
    u32_t now;
    u32_t base;
    struct sched_timeo *t;

    if (next_timeout == NULL) {
        return;
    }

    now = HAL_GetTick();
    base = next_timeout->time;

    for (t = next_timeout; t != NULL; t = t->next) {
        t->time = (t->time - base) + now;
    }
}

/** Return the time left before the next timeout is due. If no timeouts are
 * enqueued, returns 0xffffffff
 */
u32_t sched_timeouts_sleeptime(void)
{
    u32_t now;

    if (next_timeout == NULL) {
        return TIMEOUT_INFINITE;
    }

    now = HAL_GetTick();
    if (TIME_LESS_THAN(next_timeout->time, now)) {
        return 0;
    } else {
        u32_t ret = (u32_t)(next_timeout->time - now);
        return ret;
    }
}

static void test_fun(void *arg)
{
    (void)arg;
    CARD_DEBUGF(1, ("100ms timeout\n"));
    sched_timeout(100,test_fun,NULL);
}

static void sched_timeout_thread(void *arg)
{
    u32_t sleep_time;

    (void)arg;

    CARD_DEBUGF(1, ("sched timeout thread start!\n"));
    sched_timeout(100,test_fun,NULL);

    for(;;) {
        sleep_time = sched_timeouts_sleeptime();
        if (sleep_time == TIMEOUT_INFINITE) {
            osDelay(100);
        } else if (sleep_time > 0) {
            osDelay(sleep_time);
        } else {
            sched_check_timeouts();
        }
    }

}

void sched_timeout_init(void)
{
    sys_thread_new("sched_timout", sched_timeout_thread, NULL, SCHED_STACK_SIZE, osPriorityNormal);
}
