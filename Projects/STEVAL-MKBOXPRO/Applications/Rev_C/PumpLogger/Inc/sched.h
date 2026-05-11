/**
  ******************************************************************************
  * @file    sched.h
  * @brief   Cooperative tick scheduler. SysTick fires at PL_TICK_HZ; modules
  *          gate their work on `sched_should_run(N)` which returns true once
  *          every N ticks.
  ******************************************************************************
  */

#ifndef PUMPLOGGER_SCHED_H
#define PUMPLOGGER_SCHED_H

#include <stdint.h>
#include <stdbool.h>

/* Current tick count since boot. Driven by the SysTick handler. */
uint32_t sched_tick(void);

/* Sleeps the main loop until the next tick boundary (i.e. SysTick fires).
   Implemented with WFI so we hand the CPU back to the LPM logic in
   between work. Cheap; wake latency is one SysTick period. */
void sched_wait_next_tick(void);

/* Convenience: true iff the *current* tick is a multiple of `cadence`.
   Used by tasks like:
       if (sched_should_run(PL_CADENCE_LED_BLINK)) toggle_led();
   Holds true for exactly one tick out of every `cadence` ticks. */
bool sched_should_run(uint32_t cadence);

#endif /* PUMPLOGGER_SCHED_H */
