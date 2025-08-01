/**
  ******************************************************************************
  * @file    custom_mems_conf.h
  * @author  MEMS Software Solutions Team
  * @brief   This file contains definitions of the MEMS components bus interfaces for custom boards
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CUSTOM_MEMS_CONF_H
#define CUSTOM_MEMS_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"
#include "steval_mkboxpro_bus.h"
#include "steval_mkboxpro_errno.h"

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

#define USE_CUSTOM_MOTION_SENSOR_LIS2MDL_0        0U

#define USE_CUSTOM_ENV_SENSOR_STTS22H_0           0U

#define USE_CUSTOM_ENV_SENSOR_LPS22DF_0           0U

#define USE_CUSTOM_MOTION_SENSOR_LIS2DU12_0       0U

#define USE_CUSTOM_MOTION_SENSOR_LSM6DSV16X_0     0U

#define CUSTOM_LIS2MDL_0_I2C_Init BSP_I2C1_Init
#define CUSTOM_LIS2MDL_0_I2C_DeInit BSP_I2C1_DeInit
#define CUSTOM_LIS2MDL_0_I2C_ReadReg BSP_I2C1_ReadReg
#define CUSTOM_LIS2MDL_0_I2C_WriteReg BSP_I2C1_WriteReg

#define CUSTOM_STTS22H_0_I2C_Init BSP_I2C1_Init
#define CUSTOM_STTS22H_0_I2C_DeInit BSP_I2C1_DeInit
#define CUSTOM_STTS22H_0_I2C_ReadReg BSP_I2C1_ReadReg
#define CUSTOM_STTS22H_0_I2C_WriteReg BSP_I2C1_WriteReg

#define CUSTOM_LPS22DF_0_I2C_INIT BSP_I2C1_Init
#define CUSTOM_LPS22DF_0_I2C_DEINIT BSP_I2C1_DeInit
#define CUSTOM_LPS22DF_0_I2C_READ_REG BSP_I2C1_ReadReg
#define CUSTOM_LPS22DF_0_I2C_WRITE_REG BSP_I2C1_WriteReg

#define CUSTOM_LIS2DU12_0_I2C_Init BSP_I2C1_Init
#define CUSTOM_LIS2DU12_0_I2C_DeInit BSP_I2C1_DeInit
#define CUSTOM_LIS2DU12_0_I2C_ReadReg BSP_I2C1_ReadReg
#define CUSTOM_LIS2DU12_0_I2C_WriteReg BSP_I2C1_WriteReg

#define CUSTOM_LSM6DSV16X_0_I2C_Init BSP_I2C1_Init
#define CUSTOM_LSM6DSV16X_0_I2C_DeInit BSP_I2C1_DeInit
#define CUSTOM_LSM6DSV16X_0_I2C_ReadReg BSP_I2C1_ReadReg
#define CUSTOM_LSM6DSV16X_0_I2C_WriteReg BSP_I2C1_WriteReg

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_MEMS_CONF_H*/
