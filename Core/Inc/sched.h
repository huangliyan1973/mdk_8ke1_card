#ifndef SCHED_TIMEOUTS_H
#define SCHED_TIMEOUTS_H

#include "lwip/sys.h"

#define TIMEOUT_INFINITE 0xFFFFFFFF

typedef void (* sched_timeout_handler)(void *arg);

struct sched_timeo {
    struct sched_timeo *next;
    u32_t   time;
    sched_timeout_handler h;
    void    *arg;
};

void sched_timeout(u32_t msecs, sched_timeout_handler handler, void *arg);

void sched_untimeout(sched_timeout_handler handler, void *arg);

void sched_check_timeouts(void);

u32_t sched_timeouts_sleeptime(void);

void sched_timeout_init(void);

void sched_timeout_routine(void);

int sched_timeout_is_exist(sched_timeout_handler handler, void *arg);

#endif /* SCHED_TIMEOUTS_H */
