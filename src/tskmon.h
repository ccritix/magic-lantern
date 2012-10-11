#ifndef _tskmon_h_
#define _tskmon_h_

/* MAX_TASKS must be a value (1<<x) because its used to mask max values */
#define TSKMON_MAX_TASKS      0x80
#define TSKMON_MAX_TIMER_VAL  0x00100000
#define TSKMON_PCT_SCALING    1000


typedef struct
{
    uint32_t absolute;
    uint32_t relative;
    uint32_t absolute_avg;
    uint32_t relative_avg;
    uint32_t microseconds;
} taskload_t;

uint32_t tskmon_get_timer_reg();
int tskmon_update_loads(taskload_t *task_loads);
void tskmon_update_timers();
void tskmon_update_runtime(struct task *task, uint32_t active_time);
void tskmon_task_dispatch();
void tskmon_init();

#endif
