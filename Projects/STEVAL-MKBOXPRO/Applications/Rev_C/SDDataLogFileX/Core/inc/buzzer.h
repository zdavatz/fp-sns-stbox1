/**
  ******************************************************************************
  * @file    buzzer.h
  * @brief   Piezo buzzer driver for SensorTile.box PRO (PE13 / TIM1_CH3).
  ******************************************************************************
  */

#ifndef __BUZZER_H
#define __BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void Buzzer_Init(void);
void Buzzer_Beep(uint32_t freq_hz, uint16_t duration_ms);
void Buzzer_BootDone(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUZZER_H */
