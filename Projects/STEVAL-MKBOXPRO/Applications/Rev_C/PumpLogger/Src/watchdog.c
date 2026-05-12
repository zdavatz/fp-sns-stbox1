/**
  ******************************************************************************
  * @file    watchdog.c
  * @brief   IWDG @ 8 s + sensor-plausibility watchdog (PL_PLAUSIBLE_WINDOW_MS
  *          of identical bytes across every sensor → NVIC_SystemReset).
  ******************************************************************************
  */

#include "main.h"
#include "watchdog.h"
#include "sched.h"

static IWDG_HandleTypeDef hiwdg1;

static volatile uint32_t g_last_change_ms[PL_WD_N];
static volatile uint32_t g_last_hash[PL_WD_N];

void Watchdog_Init(void)
{
  for (int i = 0; i < PL_WD_N; i++) {
    g_last_hash[i]      = 0;
    g_last_change_ms[i] = HAL_GetTick();
  }

  /* LSI ≈ 32 kHz; prescaler /64, reload 4000 → period ≈ 8.0 s.
     Bumped from 2 s during Phase 3 because SD writes (especially the
     first cluster-extension on a new session) can stall up to 2 s in
     HAL_SD_WriteBlocks + sd_wait_idle, and 2 s of headroom turned
     out to be too tight. Phase 8 hardening may bring this back down
     once we batch writes more aggressively. */
  hiwdg1.Instance       = IWDG;
  hiwdg1.Init.Prescaler = IWDG_PRESCALER_64;
  hiwdg1.Init.Window    = IWDG_WINDOW_DISABLE;
  hiwdg1.Init.Reload    = 4000U;
  hiwdg1.Init.EWI       = 0;          /* no early-warning ISR — pure HW reset */
  (void)HAL_IWDG_Init(&hiwdg1);
}

void Watchdog_Feed(uint8_t id, uint32_t hash)
{
  if (id >= PL_WD_N) return;
  if (g_last_hash[id] != hash) {
    g_last_hash[id]      = hash;
    g_last_change_ms[id] = HAL_GetTick();
  }
}

void Watchdog_Tick(void)
{
  /* Kick HW watchdog every iteration. */
  HAL_IWDG_Refresh(&hiwdg1);

  if (!sched_due(PL_SCHED_PLAUSIBLE, PL_CADENCE_PLAUSIBLE)) return;

  uint32_t now = HAL_GetTick();
  for (int i = 0; i < PL_WD_N; i++) {
    if ((now - g_last_change_ms[i]) < PL_PLAUSIBLE_WINDOW_MS) {
      return;   /* at least one sensor is still producing fresh data */
    }
  }
  /* All sensors produced identical bytes for ≥ PL_PLAUSIBLE_WINDOW_MS.
     I²C/SPI silently wedged or the chips' data registers stuck — only
     way out is a clean reset. */
  NVIC_SystemReset();
}
