#ifndef ECU_ELAPSED_FAULT_TIMER_H_
#define ECU_ELAPSED_FAULT_TIMER_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t start_ms;
    bool active;
} ecu_elapsed_fault_timer_t;

static inline void ecu_elapsed_fault_timer_reset(
    ecu_elapsed_fault_timer_t *timer)
{
    if(timer != NULL)
    {
        timer->start_ms = 0u;
        timer->active = false;
    }
}

/* Returns true once condition has remained continuously asserted for at least
 * limit_ms. Tick subtraction is wrap-safe for intervals below 2^31 ms. */
static inline bool ecu_elapsed_fault_timer_update(
    ecu_elapsed_fault_timer_t *timer,
    bool condition,
    uint32_t now_ms,
    uint32_t limit_ms)
{
    if((timer == NULL) || (limit_ms == 0u))
    {
        return true;
    }
    if(!condition)
    {
        ecu_elapsed_fault_timer_reset(timer);
        return false;
    }
    if(!timer->active)
    {
        timer->active = true;
        timer->start_ms = now_ms;
        return false;
    }
    return ((uint32_t)(now_ms - timer->start_ms) >= limit_ms);
}

#endif
