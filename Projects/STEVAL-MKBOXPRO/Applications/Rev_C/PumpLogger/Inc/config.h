/**
  ******************************************************************************
  * @file    config.h
  * @brief   Single-file constants for the PumpLogger firmware. Sensor rates,
  *          buffer sizes, GATT UUIDs, watchdog periods. Everything anyone
  *          might want to tune lives here.
  ******************************************************************************
  */

#ifndef PUMPLOGGER_CONFIG_H
#define PUMPLOGGER_CONFIG_H

/* Firmware identification --------------------------------------------------*/
#define PL_FW_NAME             "PumpLogger"
#define PL_FW_VERSION          "0.1.0"
#define PL_FW_BLE_NAME         "STBoxFs"   /* 7 chars, fits BLE name budget */

/* GPIO pin map (taken from the SensorTileBoxPro BSP; we use HAL directly) --*/
#define PL_LED_GREEN_PORT      GPIOF
#define PL_LED_GREEN_PIN       GPIO_PIN_6
#define PL_LED_RED_PORT        GPIOH
#define PL_LED_RED_PIN         GPIO_PIN_11
#define PL_BUZZER_PORT         GPIOE
#define PL_BUZZER_PIN          GPIO_PIN_13
#define PL_BUZZER_TIM_AF       GPIO_AF1_TIM1

/* Cooperative scheduler ----------------------------------------------------*/
#define PL_TICK_HZ             1000U       /* SysTick @ 1 ms */

/* Task cadences (in ticks). Tasks run when `(tick_count % CADENCE) == 0`. */
#define PL_CADENCE_LED_BLINK   500U        /* heartbeat LED toggle = 0.5 Hz */

#endif /* PUMPLOGGER_CONFIG_H */
