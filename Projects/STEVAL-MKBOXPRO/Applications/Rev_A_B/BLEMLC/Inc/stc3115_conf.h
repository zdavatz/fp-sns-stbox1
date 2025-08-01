/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stc3115_conf.h
  * @author  System Research & Applications Team - Agrate/Catania Lab.
  * @brief   This file provides code for the configuration
  *          of the stc3115 component.
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
#ifndef __STC3115_CONF__H__
#define __STC3115_CONF__H__

#ifdef __cplusplus
extern "C" {
#endif

#include "steval_mkboxpro_bus.h"

/* STC3115 battery Sensor */
#define BSP_STC3115_0_I2C_INIT          BSP_I2C4_Init
#define BSP_STC3115_0_I2C_DEINIT        BSP_I2C4_DeInit
#define BSP_STC3115_0_I2C_READ_REG      BSP_I2C4_ReadReg
#define BSP_STC3115_0_I2C_WRITE_REG     BSP_I2C4_WriteReg

#ifdef __cplusplus
}
#endif

#endif /*__STC3115_CONF__H */
