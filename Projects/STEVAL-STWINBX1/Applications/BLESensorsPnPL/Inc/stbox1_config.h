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
/***************************************
  *  Remapping instance sensor defines *
  **************************************/
/* Motion Sensor Instance */
#define ACCELERO_INSTANCE        ISM330DHCX_0
#define GYRO_INSTANCE            ISM330DHCX_0
#define MAGNETO_INSTANCE         IIS2MDC_0
#define ACCELERO_INSTANCE_2      IIS2DLPC_0

/* Environmental Sensor Instance */
#define TEMPERATURE_INSTANCE    STTS22H_0
#define PRESSURE_INSTANCE       ILPS22QS_0
/*************************************/

/* For enabling the printf */
#define STBOX1_ENABLE_PRINTF

/* Blink LED Every second */
#define STBOX1_UPDATE_LED 10000
/* Update Battery Every second */
#define STBOX1_UPDATE_BATTERY 10000

#define ACC_ODR 104.0f /* ODR = 104 */
#define ACC_FS 4 /* FS = 4g */
#define GYRO_ODR 104.0f /* ODR = 104 */
#define GYRO_FS 2000 /* FS = 2000dps */
#define MAG_ODR 100.0f /* ODR = 100Hz */
#define MAG_FS 50 /* FS = 50gauss */
#define ACC_ODR_2 100.0f /* ODR = 100Hz */
#define ACC_FS_2 4 /* FS = 4g */
#define TEMP_ODR 1.0f /* ODR = 1.0Hz */
#define PRESS_ODR 25.0f /* ODR = 25.0Hz */

#include "stm32u5xx_hal.h"
/* Exported defines for TIMER -------------------------------------------------*/
extern TIM_HandleTypeDef htim1;
#define TIM_CC_HANDLE    htim1
#define TIM_CC_INSTANCE  TIM1

/***************************************
  * Don't Change the following defines *
  **************************************/

/* Package Version only numbers 0->9 */
#define STBOX1_VERSION_MAJOR '2'
#define STBOX1_VERSION_MINOR '1'
#define STBOX1_VERSION_PATCH '0'

/* Package Name */
#define STBOX1_PACKAGENAME "BLESensorsPnPL"

/* USER CODE BEGIN 1 */
/* Firmware ID */
#define STBOX1_BLUEST_SDK_FW_ID 0x34
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
