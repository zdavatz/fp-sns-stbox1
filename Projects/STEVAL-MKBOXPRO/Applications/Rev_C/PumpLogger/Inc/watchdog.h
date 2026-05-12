/**
  ******************************************************************************
  * @file    watchdog.h
  * @brief   IWDG (2 s hardware) + sensor-plausibility watchdog.
  ******************************************************************************
  */
#ifndef PL_WATCHDOG_H
#define PL_WATCHDOG_H

#include <stdint.h>

/* One-shot init: starts the IWDG. Call once after clocks are up. */
void Watchdog_Init(void);

/* Called every main-loop iteration. Kicks IWDG; runs plausibility scan. */
void Watchdog_Tick(void);

/* Feed the plausibility watchdog with a fresh sample-byte hash. Called from
   each sensor module after a successful read. `id` 0..3 maps to imu/mag/baro/fuel. */
#define PL_WD_IMU   0
#define PL_WD_MAG   1
#define PL_WD_BARO  2
#define PL_WD_FUEL  3
#define PL_WD_GPS   4
#define PL_WD_N     5
void Watchdog_Feed(uint8_t id, uint32_t hash);

#endif
