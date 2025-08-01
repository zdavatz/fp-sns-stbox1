/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   app_blesensorspnpl.h
  * @author System Research & Applications Team - Agrate/Catania Lab.
  * @brief  Header for app_blesensorspnpl.c file.
  *         This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_BLESENSORSPNPL_H__
#define __APP_BLESENSORSPNPL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"
#include "steval_mkboxpro.h"
#include "SensorTileBoxPro_conf.h"

#include "ble_manager.h"

/* Exported defines -----------------------------------------------------------*/
#define STBOX1_ERROR_INIT_BLE 1
#define STBOX1_ERROR_FLASH 2
#define STBOX1_ERROR_SENSOR 3
#define STBOX1_ERROR_HW_INIT 4
#define STBOX1_ERROR_BLE 5
#define STBOX1_ERROR_TIMER 6

/* STM32 Unique ID */
#define STM32_UUID ((uint32_t *)0x0BFA0700)

/* STM32 MCU_ID */
#define STM32_MCU_ID ((uint32_t *)0xE0044000)

/* Exported macro ------------------------------------------------------------*/
#define MCR_HEART_BIT()   \
  {                         \
    BSP_LED_On(LED_YELLOW); \
    BSP_LED_On(LED_GREEN);  \
    HAL_Delay(200);         \
    BSP_LED_Off(LED_YELLOW);\
    BSP_LED_Off(LED_GREEN); \
    HAL_Delay(400);         \
    BSP_LED_On(LED_YELLOW); \
    BSP_LED_On(LED_GREEN);  \
    HAL_Delay(200);         \
    BSP_LED_Off(LED_YELLOW);\
    BSP_LED_Off(LED_GREEN); \
    HAL_Delay(1000);        \
  }

#define MCR_HEART_BIT2()  \
  {                         \
    BSP_LED_On(LED_YELLOW); \
    BSP_LED_On(LED_RED);    \
    HAL_Delay(200);         \
    BSP_LED_Off(LED_YELLOW);\
    BSP_LED_Off(LED_RED);   \
    HAL_Delay(400);         \
    BSP_LED_On(LED_YELLOW); \
    BSP_LED_On(LED_RED);    \
    HAL_Delay(200);         \
    BSP_LED_Off(LED_YELLOW);\
    BSP_LED_Off(LED_RED);   \
    HAL_Delay(1000);        \
  }

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  STBOX1_ACC_LSM6DSV16X = 0,
  STBOX1_ACC_LIS2DU12 = 1
} STBOX1_Acc_t;

/* Exported Variables --------------------------------------------------------*/
extern int32_t CurrentActiveBank;
extern FinishGood_TypeDef FinishGood;

extern void *HandleGGComponent;

extern STBOX1_Acc_t CurrentAccType;

/* Exported Functions Prototypes ---------------------------------------------*/
void MX_BLESensorsPnPL_Init(void);
void MX_BLESensorsPnPL_Process(void);

extern void STBOX1_Error_Handler(int32_t ErrorCode, char *File, int32_t Line);

#ifdef __cplusplus
}
#endif

#endif /*__APP_BLESENSORSPNPL_H__ */
