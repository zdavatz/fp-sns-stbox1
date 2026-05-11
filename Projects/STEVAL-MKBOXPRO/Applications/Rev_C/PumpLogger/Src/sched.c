/**
  ******************************************************************************
  * @file    sched.c
  * @brief   SysTick-driven cooperative scheduler.
  ******************************************************************************
  */

#include "main.h"
#include "sched.h"

static volatile uint32_t g_tick_count;

/* Called by HAL_IncTick() (which the SysTick_Handler calls). Lets us keep
   our own counter independent of HAL_GetTick(), so tasks aren't affected
   if HAL's clock book-keeping ever needs to skip a beat. */
void HAL_SYSTICK_Callback(void)
{
  g_tick_count++;
}

uint32_t sched_tick(void)
{
  return g_tick_count;
}

void sched_wait_next_tick(void)
{
  uint32_t before = g_tick_count;
  /* WFI suspends the CPU until the next interrupt. SysTick wakes us within
     ≤ 1 ms. Any other interrupt (when we eventually have them) would also
     wake us, which is fine — the loop body re-checks the work it has. */
  while (g_tick_count == before) {
    __WFI();
  }
}

bool sched_should_run(uint32_t cadence)
{
  if (cadence == 0) return true;
  return (g_tick_count % cadence) == 0;
}
