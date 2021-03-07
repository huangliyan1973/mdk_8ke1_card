#include "main.h"
#include "sched.h"

#define LOG_TAG              "sch_timer"
#define LOG_LVL              LOG_LVL_DBG
#include "ulog.h"

#define SCHED_MAX_TIMEOUT   0x7fffffff
#define SCHED_STACK_SIZE      256*4

#define TIME_LESS_THAN(t, compare_to)   ( (((u32_t)((t)-(compare_to))) > SCHED_MAX_TIMEOUT) ? 1 : 0)

static struct sched_timeo *next_timeout;

//static u32_t current_timeout_due_time;

static sys_mutex_t  time_lock;

#define LOCK_TIME_CORE()      sys_mutex_lock(&time_lock)
#define UNLOCK_TIME_CORE()    sys_mutex_unlock(&time_lock)

static void sched_timeout_abs(u32_t abs_time, sched_timeout_handler handler, void *arg)
{
    struct sched_timeo *timeout, *t;

    LOCK_TIME_CORE();
    timeout = (struct sched_timeo *)pvPortMalloc(sizeof(struct sched_timeo));

    if (timeout == NULL) {
        printf("sched_timeout: timeout != NULL, FreeRtos memory pool is empty");
        UNLOCK_TIME_CORE();
        return;
    }

    timeout->next = NULL;
    timeout->h = handler;
    timeout->arg = arg;
    timeout->time = abs_time;

    if (next_timeout == NULL) {
        next_timeout = timeout;
        UNLOCK_TIME_CORE();
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
    UNLOCK_TIME_CORE();
}

void sched_timeout(u32_t msecs, sched_timeout_handler handler, void *arg)
{
    /* First delete the timer */
    sched_untimeout(handler, arg);

    u32_t next_timeout_time;

    next_timeout_time = (u32_t)(HAL_GetTick() + msecs); /* overflow handled by TIME_LESS_THAN macro */

    sched_timeout_abs(next_timeout_time, handler, arg);
}

void sched_untimeout(sched_timeout_handler handler, void *arg)
{
    struct sched_timeo *prev_t, *t;

    LOCK_TIME_CORE();
    if (next_timeout == NULL) {
        UNLOCK_TIME_CORE();
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
            UNLOCK_TIME_CORE();
            return;
        }
    }

    UNLOCK_TIME_CORE();
}

void sched_check_timeouts(void)
{
    u32_t now;

    LOCK_TIME_CORE();

    now = HAL_GetTick();
 
    do {
        struct sched_timeo *tmptimeout;
        sched_timeout_handler handler;
        void *arg;

        tmptimeout = next_timeout;
        if (tmptimeout == NULL) {
            break;
        }

        if (TIME_LESS_THAN(now, tmptimeout->time)) {
            break;
        }

        next_timeout = tmptimeout->next;
        handler = tmptimeout->h;
        arg = tmptimeout->arg;
        //current_timeout_due_time = tmptimeout->time;
        vPortFree(tmptimeout);
        tmptimeout = NULL;

        if (handler != NULL) {
            UNLOCK_TIME_CORE();
            handler(arg);
            LOCK_TIME_CORE();
        }

    } while (1);

    UNLOCK_TIME_CORE();
}

/** Return the time left before the next timeout is due. If no timeouts are
 * enqueued, returns 0xffffffff
 */
u32_t sched_timeouts_sleeptime(void)
{
    u32_t now;
    u32_t ret;

    LOCK_TIME_CORE();

    now = HAL_GetTick();
    if (next_timeout == NULL) {
        ret = TIMEOUT_INFINITE;
    } else if (TIME_LESS_THAN(next_timeout->time, now)) {
        ret = 0;
    } else {
        ret = (u32_t)(next_timeout->time - now);
    }

    UNLOCK_TIME_CORE();
    return ret;
}
#if 0
static void test_fun(void *arg)
{
    (void)arg;
    CARD_DEBUGF(1, ("100ms timeout\n"));
    sched_timeout(100,test_fun,NULL);
}
#endif

static void sched_timeout_thread(void *arg)
{
    (void)arg;

    if (sys_mutex_new(&time_lock) != ERR_OK) {
        printf("mem allocate for time lock error!\n");
    }

    LOG_D("sched_timer_start!");
    TickType_t  xPeriod = pdMS_TO_TICKS(10); //10ms 运行一次
	TickType_t xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();

    for(;;) {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        sched_check_timeouts();

    }

}

void sched_timeout_init(void)
{
    sys_thread_new("sched_timout", sched_timeout_thread, NULL, SCHED_STACK_SIZE, osPriorityNormal+3);
#if 0
	if (sys_mutex_new(&time_lock) != ERR_OK) {
        CARD_ASSERT("mem allocate for time lock error!\n", 0);
    }
#endif
}

void sched_timeout_routine(void)
{
	u32_t sleep_time;
	
	sleep_time = sched_timeouts_sleeptime();
	
	if (sleep_time == 0) {
		sched_check_timeouts();
	}
}
