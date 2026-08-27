#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ext_drivers/elapsed_fault_timer.h"

#define CHECK(condition) do { \
    if(!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while(0)

int main(void)
{
    ecu_elapsed_fault_timer_t timer = {0};

    /* First observation starts the interval and does not fault. */
    CHECK(!ecu_elapsed_fault_timer_update(&timer, true, 1000u, 90u));
    CHECK(timer.active);
    CHECK(timer.start_ms == 1000u);

    CHECK(!ecu_elapsed_fault_timer_update(&timer, true, 1089u, 90u));
    CHECK(ecu_elapsed_fault_timer_update(&timer, true, 1090u, 90u));

    /* A healthy observation resets all persistence. */
    CHECK(!ecu_elapsed_fault_timer_update(&timer, false, 1091u, 90u));
    CHECK(!timer.active);
    CHECK(timer.start_ms == 0u);

    /* Tick subtraction remains correct across uint32 wrap. */
    CHECK(!ecu_elapsed_fault_timer_update(&timer, true,
                                           UINT32_MAX - 39u, 90u));
    CHECK(!ecu_elapsed_fault_timer_update(&timer, true, 49u, 90u));
    CHECK(ecu_elapsed_fault_timer_update(&timer, true, 50u, 90u));

    /* Invalid configuration fails closed. */
    CHECK(ecu_elapsed_fault_timer_update(NULL, true, 0u, 90u));
    CHECK(ecu_elapsed_fault_timer_update(&timer, true, 0u, 0u));

    puts("PASS elapsed-time plausibility timer boundary tests");
    return EXIT_SUCCESS;
}
