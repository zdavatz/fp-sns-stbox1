/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stbox1_config.h
  * @author  System Research & Applications Team - Agrate/Catania Lab.
  * @brief   FP-SNS-STBOX1 configuration
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
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STBOX1_CONFIG_H
#define __STBOX1_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Exported define ------------------------------------------------------------*/

/* For enabling the printf */
#define STBOX1_ENABLE_PRINTF

/* Blink LED Every second */
#define STBOX1_UPDATE_LED 10000
/* Update Environmental Features @2HZ */
#define STBOX1_UPDATE_ENV 5000
/* Update Inertial Features @20HZ */
#define STBOX1_UPDATE_INV 500

/* For Enabling Bip Sound at Start up */
#define STBOX1_ENABLE_START_BIP

#include "stm32u5xx_hal.h"
/* Exported defines for TIMER -------------------------------------------------*/
extern TIM_HandleTypeDef htim1;
#define TIM_CC_HANDLE    htim1
#define TIM_CC_INSTANCE  TIM1
#define BLEDUALPROG_TIMx_CLK_ENABLE              __HAL_RCC_TIM1_CLK_ENABLE
#define BLEDUALPROG_TIMx_FORCE_RESET             __HAL_RCC_TIM1_FORCE_RESET
#define BLEDUALPROG_TIMx_RELEASE_RESET           __HAL_RCC_TIM1_RELEASE_RESET
#define BLEDUALPROG_GPIO_AF1_TIMx                GPIO_AF1_TIM1

/* For enabling Secure connection */
#define STBOX1_BLE_SECURE_CONNECTION

/***************************************
  * Don't Change the following defines *
  **************************************/

/* Package Version only numbers 0->9 */
#define STBOX1_VERSION_MAJOR '2'
#define STBOX1_VERSION_MINOR '1'
#define STBOX1_VERSION_PATCH '0'

/* Package Name */
#define STBOX1_PACKAGENAME "BLEDualProgram"

/* USER CODE BEGIN 1 */

/* Firmware IDs */
#define STBOX1C_BLUEST_SDK_FW_ID 0x03

/* USER CODE END 1 */

#ifdef STBOX1_ENABLE_PRINTF
#define STBOX1_PRINTF(...) printf(__VA_ARGS__)
#else /* STBOX1_ENABLE_PRINTF */
#define STBOX1_PRINTF(...)
#endif /* STBOX1_ENABLE_PRINTF */

#ifdef __cplusplus
}
#endif

#endif /* __STBOX1_CONFIG_H */
