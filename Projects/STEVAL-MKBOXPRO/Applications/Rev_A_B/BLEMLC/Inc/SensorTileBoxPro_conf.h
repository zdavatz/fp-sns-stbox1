/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    SensorTileBoxPro_conf.h
  * @author  System Research & Applications Team - Agrate/Catania Lab.
  * @brief   Configuration file
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
#ifndef SENSORTILEBOXPRO_CONF_H
#define SENSORTILEBOXPRO_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"
#include "steval_mkboxpro_bus.h"
#include "steval_mkboxpro_errno.h"
#include "custom_mems_conf.h"
#include "stc3115_conf.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup SENSORTILEBOXPRO
  * @{
  */

/** @defgroup SENSORTILEBOXPRO_CONFIG Config
  * @{
  */

/** @defgroup SENSORTILEBOXPRO_CONFIG_Exported_Constants
  * @{
  */

/* FINISHA --> BOARD_ID = 0x0DU
   FINISHB --> BOARD_ID = 0x11U */
typedef enum
{
  FINISHA = 0,
  FINISHB = 1,
  FINISH_ERROR = 2
} FinishGood_TypeDef;

/*Error code*/
#define BSP_NOT_IMPLEMENTED              -12

/* UART4 Baud rate in bps  */
#define BUS_UART4_BAUDRATE                  115200U /* baud rate of UARTn = 115200 baud */

/**  Definition for LSM6DSV16X INTERRUPT PIN  **/
extern EXTI_HandleTypeDef hexti4;
#define H_EXTI_4                                   hexti4
#define H_EXTI_INT_LSM6DSV16X                      hexti4
#define INT_LSM6DSV16X_PIN                         GPIO_PIN_4
#define INT_LSM6DSV16X_GPIO_PORT                   GPIOA
#define INT_LSM6DSV16X_GPIO_CLK_ENABLE()           __HAL_RCC_GPIOA_CLK_ENABLE()
#define INT_LSM6DSV16X_GPIO_CLK_DISABLE()          __HAL_RCC_GPIOA_CLK_DISABLE()
#define INT_LSM6DSV16X_EXTI_LINE                   EXTI_LINE_4
#define INT_LSM6DSV16X_EXTI_IRQ_N                  EXTI4_IRQn
#define EXTI_LSM6DSV16X_IRQHandler                 EXTI4_IRQHandler

#define USE_MOTION_SENSOR_LIS2MDL_0        1U

#define USE_ENV_SENSOR_STTS22H_0           0U

#define USE_ENV_SENSOR_LPS22DF_0           0U

#define USE_MOTION_SENSOR_LIS2DU12_0       1U

#define USE_MOTION_SENSOR_LSM6DSV16X_0     1U

#define BSP_NFCTAG_INSTANCE         0U

#if (USE_MOTION_SENSOR_LIS2MDL_0 + USE_MOTION_SENSOR_LSM6DSV16X_0 + USE_MOTION_SENSOR_LIS2DU12_0 == 0 )
#undef USE_MOTION_SENSOR_LSM6DSV16X_0
#define USE_MOTION_SENSOR_LSM6DSV16X_0     1U
#endif /* (USE_MOTION_SENSOR_LIS2MDL_0 + USE_MOTION_SENSOR_LSM6DSV16X_0 + USE_MOTION_SENSOR_LIS2DU12_0 == 0 ) */

#if (USE_ENV_SENSOR_STTS22H_0 + USE_ENV_SENSOR_LPS22DF_0 == 0 )
#undef USE_ENV_SENSOR_STTS22H_0
#define USE_ENV_SENSOR_STTS22H_0     1U
#endif /* (USE_ENV_SENSOR_STTS22H_0 + USE_ENV_SENSOR_LPS22DF_0 == 0 ) */

/* For using LSM6DSV16X and LIS2DU12 with I2C */
#define ALL_SENSORS_I2C

/* LIS2MDL magneto Sensor */
#define BSP_LIS2MDL_0_I2C_INIT            CUSTOM_LIS2MDL_0_I2C_Init
#define BSP_LIS2MDL_0_I2C_DEINIT          CUSTOM_LIS2MDL_0_I2C_DeInit
#define BSP_LIS2MDL_0_I2C_READ_REG        CUSTOM_LIS2MDL_0_I2C_ReadReg
#define BSP_LIS2MDL_0_I2C_WRITE_REG       CUSTOM_LIS2MDL_0_I2C_WriteReg

/* LPS22DF Pressure Sensor */
#define BSP_LPS22DF_0_I2C_INIT            CUSTOM_LPS22DF_0_I2C_INIT
#define BSP_LPS22DF_0_I2C_DEINIT          CUSTOM_LPS22DF_0_I2C_DEINIT
#define BSP_LPS22DF_0_I2C_READ_REG        CUSTOM_LPS22DF_0_I2C_READ_REG
#define BSP_LPS22DF_0_I2C_WRITE_REG       CUSTOM_LPS22DF_0_I2C_WRITE_REG

/* STTS22H Temperature Sensor */
#define BSP_STTS22H_0_I2C_INIT            CUSTOM_STTS22H_0_I2C_Init
#define BSP_STTS22H_0_I2C_DEINIT          CUSTOM_STTS22H_0_I2C_DeInit
#define BSP_STTS22H_0_I2C_READ_REG        CUSTOM_STTS22H_0_I2C_ReadReg
#define BSP_STTS22H_0_I2C_WRITE_REG       CUSTOM_STTS22H_0_I2C_WriteReg

/* LIS2DU12 acc Sensor */
#define USE_LIS2DU12_I2C
#define BSP_LIS2DU12_0_I2C_INIT           CUSTOM_LIS2DU12_0_I2C_Init
#define BSP_LIS2DU12_0_I2C_DEINIT         CUSTOM_LIS2DU12_0_I2C_DeInit
#define BSP_LIS2DU12_0_I2C_READ_REG       CUSTOM_LIS2DU12_0_I2C_ReadReg
#define BSP_LIS2DU12_0_I2C_WRITE_REG      CUSTOM_LIS2DU12_0_I2C_WriteReg

/* LSM6DSV16X acc - gyro - qvar Sensor */
#define USE_LSM6DSV16X_I2C
#define BSP_LSM6DSV16X_0_I2C_INIT               CUSTOM_LSM6DSV16X_0_I2C_Init
#define BSP_LSM6DSV16X_0_I2C_DEINIT             CUSTOM_LSM6DSV16X_0_I2C_DeInit
#define BSP_LSM6DSV16X_0_I2C_READ_REG           CUSTOM_LSM6DSV16X_0_I2C_ReadReg
#define BSP_LSM6DSV16X_0_I2C_WRITE_REG          CUSTOM_LSM6DSV16X_0_I2C_WriteReg

/* ST25DV nfc Device */
#define BSP_ST25DV_I2C_INIT                     BSP_I2C2_Init
#define BSP_ST25DV_I2C_DEINIT                   BSP_I2C2_DeInit
#define BSP_ST25DV_I2C_READ_REG_16              BSP_I2C2_ReadReg16
#define BSP_ST25DV_I2C_WRITE_REG_16             BSP_I2C2_WriteReg16
#define BSP_ST25DV_I2C_RECV                     BSP_I2C2_Recv
#define BSP_ST25DV_I2C_IS_READY                 BSP_I2C2_IsReady

/* ST25DVXXKC nfc Device */
#define BSP_ST25DVXXKC_I2C_INIT                 BSP_I2C2_Init
#define BSP_ST25DVXXKC_I2C_DEINIT               BSP_I2C2_DeInit
#define BSP_ST25DVXXKC_I2C_READ_REG_16          BSP_I2C2_ReadReg16
#define BSP_ST25DVXXKC_I2C_WRITE_REG_16         BSP_I2C2_WriteReg16
#define BSP_ST25DVXXKC_I2C_RECV                 BSP_I2C2_Recv
#define BSP_ST25DVXXKC_I2C_IS_READY             BSP_I2C2_IsReady

/* nfctag GPO pin */
extern EXTI_HandleTypeDef                       hexti12;
#define H_EXTI_12                               hexti12
#define GPO_EXTI                                hexti12
#define BSP_GPO_PIN                             GPIO_PIN_12
#define BSP_GPO_GPIO_PORT                       GPIOE
#define BSP_GPO_EXTI_LINE                       EXTI_LINE_12
#define BSP_GPO_EXTI_IRQN                       EXTI12_IRQn
#define BSP_GPO_CLK_ENABLE()                    __HAL_RCC_GPIOE_CLK_ENABLE()
#define BSP_GPO_EXTI_IRQHANDLER                 EXTI12_IRQHandler

/**  Definition for SD DETECT INTERRUPT PIN  **/
extern EXTI_HandleTypeDef hexti5;
#define H_EXTI_5                                hexti5
#define H_EXTI_SD_DETECT                        hexti5
#define BSP_SD_DETECT_PIN                       GPIO_PIN_5
#define BSP_SD_DETECT_GPIO_PORT                 GPIOC
#define BSP_SD_DETECT_GPIO_CLK_ENABLE()         __HAL_RCC_GPIOC_CLK_ENABLE()
#define BSP_SD_DETECT_GPIO_CLK_DISABLE()        __HAL_RCC_GPIOC_CLK_DISABLE()
#define BSP_SD_DETECT_EXTI_LINE                 EXTI_LINE_5
#define BSP_SD_DETECT_EXTI_IRQN                 EXTI5_IRQn
#define BSP_SD_DETECT_IRQHANDLER                EXTI5_IRQHandler

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif
#endif  /* SENSORTILEBOXPRO_CONF_H */

