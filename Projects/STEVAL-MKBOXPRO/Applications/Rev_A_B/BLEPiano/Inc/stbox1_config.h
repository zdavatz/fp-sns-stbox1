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
/* Update Battery Every second */
#define STBOX1_UPDATE_BATTERY 10000

#include "stm32u5xx_hal.h"
/* Exported defines for TIMER -------------------------------------------------*/
extern TIM_HandleTypeDef htim1;
#define TIM_CC_HANDLE    htim1
#define TIM_CC_INSTANCE  TIM1
#define BLEPIANO_TIMx_CLK_ENABLE                 __HAL_RCC_TIM1_CLK_ENABLE
#define BLEPIANO_TIMx_FORCE_RESET                __HAL_RCC_TIM1_FORCE_RESET
#define BLEPIANO_TIMx_RELEASE_RESET              __HAL_RCC_TIM1_RELEASE_RESET
#define BLEPIANO_GPIO_AF1_TIMx                   GPIO_AF1_TIM1

/***************************************
  * Don't Change the following defines *
  **************************************/

/* Package Version only numbers 0->9 */
#define STBOX1_VERSION_MAJOR '2'
#define STBOX1_VERSION_MINOR '1'
#define STBOX1_VERSION_PATCH '0'

/* Package Name */
#define STBOX1_PACKAGENAME "BLEPiano"

/* USER CODE BEGIN 1 */

/* Firmware IDs */
#define STBOX1A_BLUEST_SDK_FW_ID 0x20
#define STBOX1B_BLUEST_SDK_FW_ID 0x1B

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
