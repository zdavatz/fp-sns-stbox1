/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    STWIN.box_conf.h
  * @author  System Research & Applications Team - Agrate/Catania Lab.
  * @brief   STWIN.box configuration
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
#ifndef STWIN_BOX_CONF_H__
#define STWIN_BOX_CONF_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"
#include "steval_stwinbx1_bus.h"
#include "steval_stwinbx1_errno.h"
#include "custom_mems_conf.h"

/* ILPS22QS Pressure-Temperature-Qvar Sensor */
#define BSP_ILPS22QS_0_I2C_INIT            CUSTOM_ILPS22QS_0_I2C_Init
#define BSP_ILPS22QS_0_I2C_DEINIT          CUSTOM_ILPS22QS_0_I2C_DeInit
#define BSP_ILPS22QS_0_I2C_READ_REG        CUSTOM_ILPS22QS_0_I2C_ReadReg
#define BSP_ILPS22QS_0_I2C_WRITE_REG       CUSTOM_ILPS22QS_0_I2C_WriteReg

/* STTS22H Temperature Sensor */
extern EXTI_HandleTypeDef hexti5;
#define H_EXTI_5         hexti5
#define H_EXTI_INT_STTS22H                      hexti5
#define STTS22H_INT_EXTI_LINE                   EXTI_LINE_5
#define BSP_STTS22H_INT_GPIO_CLK_ENABLE()       __HAL_RCC_GPIOF_CLK_ENABLE()
#define BSP_STTS22H_INT_PORT                    GPIOF
#define BSP_STTS22H_INT_PIN                     GPIO_PIN_5
#define BSP_STTS22H_INT_EXTI_IRQn               EXTI5_IRQn
#ifndef BSP_STTS22H_INT_EXTI_IRQ_PP
#define BSP_STTS22H_INT_EXTI_IRQ_PP             7
#endif /* STTS22H_INT_EXTI_LINE */
#ifndef BSP_STTS22H_INT_EXTI_IRQ_SP
#define BSP_STTS22H_INT_EXTI_IRQ_SP             0
#endif /* BSP_STTS22H_INT_EXTI_IRQ_SP */
#define BSP_STTS22H_0_I2C_INIT                  CUSTOM_STTS22H_0_I2C_Init
#define BSP_STTS22H_0_I2C_DEINIT                CUSTOM_STTS22H_0_I2C_DeInit
#define BSP_STTS22H_0_I2C_READ_REG              CUSTOM_STTS22H_0_I2C_ReadReg
#define BSP_STTS22H_0_I2C_WRITE_REG             CUSTOM_STTS22H_0_I2C_WriteReg

/* IIS2MDC magneto Sensor */
#define BSP_IIS2MDC_0_I2C_INIT                  CUSTOM_IIS2MDC_0_I2C_Init
#define BSP_IIS2MDC_0_I2C_DEINIT                CUSTOM_IIS2MDC_0_I2C_DeInit
#define BSP_IIS2MDC_0_I2C_READ_REG              CUSTOM_IIS2MDC_0_I2C_ReadReg
#define BSP_IIS2MDC_0_I2C_WRITE_REG             CUSTOM_IIS2MDC_0_I2C_WriteReg

/* IIS2DLPC acc Sensor */
extern EXTI_HandleTypeDef hexti1;
#define H_EXTI_1         hexti1
#define H_EXTI_INT1_IIS2DLPC                      hexti1
#define IIS2DLPC_INT1_EXTI_LINE                   EXTI_LINE_1
#define BSP_IIS2DLPC_INT1_GPIO_CLK_ENABLE()       __HAL_RCC_GPIOF_CLK_ENABLE()
#define BSP_IIS2DLPC_INT1_PORT                    GPIOF
#define BSP_IIS2DLPC_INT1_PIN                     GPIO_PIN_1
#define BSP_IIS2DLPC_INT1_EXTI_IRQn               EXTI1_IRQn
#ifndef BSP_IIS2DLPC_INT1_EXTI_IRQ_PP
#define BSP_IIS2DLPC_INT1_EXTI_IRQ_PP             7
#endif /* BSP_IIS2DLPC_INT1_EXTI_IRQ_PP */
#ifndef BSP_IIS2DLPC_INT1_EXTI_IRQ_SP
#define BSP_IIS2DLPC_INT1_EXTI_IRQ_SP             0
#endif /* BSP_IIS2DLPC_INT1_EXTI_IRQ_SP */
extern EXTI_HandleTypeDef hexti2;
#define H_EXTI_2         hexti2
#define H_EXTI_INT2_IIS2DLPC                      hexti2
#define IIS2DLPC_INT2_EXTI_LINE                   EXTI_LINE_2
#define BSP_IIS2DLPC_INT2_GPIO_CLK_ENABLE()       __HAL_RCC_GPIOF_CLK_ENABLE()
#define BSP_IIS2DLPC_INT2_PORT                    GPIOF
#define BSP_IIS2DLPC_INT2_PIN                     GPIO_PIN_2
#define BSP_IIS2DLPC_INT2_EXTI_IRQn               EXTI2_IRQn
#ifndef BSP_IIS2DLPC_INT2_EXTI_IRQ_PP
#define BSP_IIS2DLPC_INT2_EXTI_IRQ_PP             7
#endif /* BSP_IIS2DLPC_INT2_EXTI_IRQ_PP */
#ifndef BSP_IIS2DLPC_INT2_EXTI_IRQ_SP
#define BSP_IIS2DLPC_INT2_EXTI_IRQ_SP             0
#endif /* BSP_IIS2DLPC_INT2_EXTI_IRQ_SP */
#define BSP_IIS2DLPC_0_SPI_INIT                  CUSTOM_IIS2DLPC_0_SPI_Init
#define BSP_IIS2DLPC_0_SPI_DEINIT                CUSTOM_IIS2DLPC_0_SPI_DeInit
#define BSP_IIS2DLPC_0_SPI_SEND                  CUSTOM_IIS2DLPC_0_SPI_Send
#define BSP_IIS2DLPC_0_SPI_RECV                  CUSTOM_IIS2DLPC_0_SPI_Recv
#define BSP_IIS2DLPC_CS_PORT                     CUSTOM_IIS2DLPC_0_CS_PORT
#define BSP_IIS2DLPC_CS_PIN                      CUSTOM_IIS2DLPC_0_CS_PIN

/* IIS3DWB acc Sensor */
extern EXTI_HandleTypeDef hexti15;
#define H_EXTI_15          hexti15
#define H_EXTI_INT1_IIS3DWB                       hexti15
#define IIS3DWB_INT1_EXTI_LINE                    EXTI_LINE_15
#define BSP_IIS3DWB_INT1_GPIO_CLK_ENABLE()        __HAL_RCC_GPIOF_CLK_ENABLE()
#define BSP_IIS3DWB_INT1_PORT                     GPIOF
#define BSP_IIS3DWB_INT1_PIN                      GPIO_PIN_15
#define BSP_IIS3DWB_INT1_EXTI_IRQn                EXTI15_IRQn
#ifndef BSP_IIS3DWB_INT1_EXTI_IRQ_PP
#define BSP_IIS3DWB_INT1_EXTI_IRQ_PP              7
#endif /* BSP_IIS3DWB_INT1_EXTI_IRQ_PP */
#ifndef BSP_IIS3DWB_INT1_EXTI_IRQ_SP
#define BSP_IIS3DWB_INT1_EXTI_IRQ_SP              0
#endif /* BSP_IIS3DWB_INT1_EXTI_IRQ_SP */
#define BSP_IIS3DWB_0_SPI_INIT                   CUSTOM_IIS3DWB_0_SPI_Init
#define BSP_IIS3DWB_0_SPI_DEINIT                 CUSTOM_IIS3DWB_0_SPI_DeInit
#define BSP_IIS3DWB_0_SPI_SEND                   CUSTOM_IIS3DWB_0_SPI_Send
#define BSP_IIS3DWB_0_SPI_RECV                   CUSTOM_IIS3DWB_0_SPI_Recv
#define BSP_IIS3DWB_CS_PORT                      CUSTOM_IIS3DWB_0_CS_PORT
#define BSP_IIS3DWB_CS_PIN                       CUSTOM_IIS3DWB_0_CS_PIN

/* ISM330DHCX acc - gyro Sensor */
extern EXTI_HandleTypeDef hexti8;
#define H_EXTI_8         hexti8
#define H_EXTI_INT1_ISM330DHCX                      hexti8
#define ISM330DHCX_INT1_EXTI_LINE                   EXTI_LINE_8
#define BSP_ISM330DHCX_INT1_GPIO_CLK_ENABLE()       __HAL_RCC_GPIOB_CLK_ENABLE()
#define BSP_ISM330DHCX_INT1_PORT                    GPIOB
#define BSP_ISM330DHCX_INT1_PIN                     GPIO_PIN_8
#define BSP_ISM330DHCX_INT1_EXTI_IRQn               EXTI8_IRQn
#ifndef BSP_ISM330DHCX_INT1_EXTI_IRQ_PP
#define BSP_ISM330DHCX_INT1_EXTI_IRQ_PP             7
#endif /* BSP_ISM330DHCX_INT1_EXTI_IRQ_PP */
#ifndef BSP_ISM330DHCX_INT1_EXTI_IRQ_SP
#define BSP_ISM330DHCX_INT1_EXTI_IRQ_SP             0
#endif /* BSP_ISM330DHCX_INT1_EXTI_IRQ_SP */
extern EXTI_HandleTypeDef hexti4;
#define H_EXTI_4         hexti4
#define H_EXTI_INT2_ISM330DHCX                      hexti4
#define ISM330DHCX_INT2_EXTI_LINE                   EXTI_LINE_4
#define BSP_ISM330DHCX_INT2_GPIO_CLK_ENABLE()       __HAL_RCC_GPIOF_CLK_ENABLE()
#define BSP_ISM330DHCX_INT2_PORT                    GPIOF
#define BSP_ISM330DHCX_INT2_PIN                     GPIO_PIN_4
#define BSP_ISM330DHCX_INT2_EXTI_IRQn               EXTI4_IRQn
#ifndef BSP_ISM330DHCX_INT2_EXTI_IRQ_PP
#define BSP_ISM330DHCX_INT2_EXTI_IRQ_PP             7
#endif /* BSP_ISM330DHCX_INT2_EXTI_IRQ_PP */
#ifndef BSP_ISM330DHCX_INT2_EXTI_IRQ_SP
#define BSP_ISM330DHCX_INT2_EXTI_IRQ_SP             0
#endif /* BSP_ISM330DHCX_INT2_EXTI_IRQ_SP */
#define BSP_ISM330DHCX_0_SPI_INIT                  CUSTOM_ISM330DHCX_0_SPI_Init
#define BSP_ISM330DHCX_0_SPI_DEINIT                CUSTOM_ISM330DHCX_0_SPI_DeInit
#define BSP_ISM330DHCX_0_SPI_SEND                  CUSTOM_ISM330DHCX_0_SPI_Send
#define BSP_ISM330DHCX_0_SPI_RECV                  CUSTOM_ISM330DHCX_0_SPI_Recv
#define BSP_ISM330DHCX_CS_PORT                     CUSTOM_ISM330DHCX_0_CS_PORT
#define BSP_ISM330DHCX_CS_PIN                      CUSTOM_ISM330DHCX_0_CS_PIN

/* IIS2ICLX acc Sensor */
extern EXTI_HandleTypeDef hexti3;
#define H_EXTI_3           hexti3
#define H_EXTI_INT1_IIS2ICLX                        hexti3
#define IIS2ICLX_INT1_EXTI_LINE                     EXTI_LINE_3
#define BSP_IIS2ICLX_INT1_GPIO_CLK_ENABLE()         __HAL_RCC_GPIOF_CLK_ENABLE()
#define BSP_IIS2ICLX_INT1_PORT                      GPIOF
#define BSP_IIS2ICLX_INT1_PIN                       GPIO_PIN_3
#define BSP_IIS2ICLX_INT1_EXTI_IRQn                 EXTI3_IRQn
#ifndef BSP_IIS2ICLX_INT1_EXTI_IRQ_PP
#define BSP_IIS2ICLX_INT1_EXTI_IRQ_PP               7
#endif /* BSP_IIS2ICLX_INT1_EXTI_IRQ_PP */
#ifndef BSP_IIS2ICLX_INT1_EXTI_IRQ_SP
#define BSP_IIS2ICLX_INT1_EXTI_IRQ_SP               0
#endif /* BSP_IIS2ICLX_INT1_EXTI_IRQ_SP */
extern EXTI_HandleTypeDef hexti11;
#define H_EXTI_11           hexti11
#define H_EXTI_INT2_IIS2ICLX                        hexti11
#define IIS2ICLX_INT2_EXTI_LINE                     EXTI_LINE_11
#define BSP_IIS2ICLX_INT2_GPIO_CLK_ENABLE()         __HAL_RCC_GPIOF_CLK_ENABLE()
#define BSP_IIS2ICLX_INT2_PORT                      GPIOF
#define BSP_IIS2ICLX_INT2_PIN                       GPIO_PIN_11
#define BSP_IIS2ICLX_INT2_EXTI_IRQn                 EXTI11_IRQn
#ifndef BSP_IIS2ICLX_INT2_EXTI_IRQ_PP
#define BSP_IIS2ICLX_INT2_EXTI_IRQ_PP               7
#endif /* BSP_IIS2ICLX_INT2_EXTI_IRQ_PP */
#ifndef BSP_IIS2ICLX_INT2_EXTI_IRQ_SP
#define BSP_IIS2ICLX_INT2_EXTI_IRQ_SP               0
#endif /* BSP_IIS2ICLX_INT2_EXTI_IRQ_SP */
#define BSP_IIS2ICLX_0_SPI_INIT                    CUSTOM_IIS2ICLX_0_SPI_Init
#define BSP_IIS2ICLX_0_SPI_DEINIT                  CUSTOM_IIS2ICLX_0_SPI_DeInit
#define BSP_IIS2ICLX_0_SPI_SEND                    CUSTOM_IIS2ICLX_0_SPI_Send
#define BSP_IIS2ICLX_0_SPI_RECV                    CUSTOM_IIS2ICLX_0_SPI_Recv
#define BSP_IIS2ICLX_CS_PORT                       CUSTOM_IIS2ICLX_0_CS_PORT
#define BSP_IIS2ICLX_CS_PIN                        CUSTOM_IIS2ICLX_0_CS_PIN

/* EXT_SPI */
#define BSP_EXT_SPI_CS_GPIO_CLK_ENABLE()           __GPIOA_CLK_ENABLE()
#define BSP_EXT_SPI_CS_PORT                        GPIOA
#define BSP_EXT_SPI_CS_PIN                         GPIO_PIN_15

/**  Definition for BSP POWER BUTTON   **/
extern EXTI_HandleTypeDef hexti10;
#define H_EXTI_10             hexti10
#define H_EXTI_POWER_BUTTON                        hexti10
#define POWER_BUTTON_PIN                           GPIO_PIN_10
#define POWER_BUTTON_GPIO_PORT                     GPIOD
#define POWER_BUTTON_GPIO_CLK_ENABLE()             __HAL_RCC_GPIOD_CLK_ENABLE()
#define POWER_BUTTON_GPIO_CLK_DISABLE()            __HAL_RCC_GPIOD_CLK_DISABLE()
#define POWER_BUTTON_EXTI_LINE                     EXTI_LINE_10
#define POWER_BUTTON_EXTI_IRQ_N                    EXTI10_IRQn

/**  Definition for acceleration sensor SPI **/
#define HANDLE_ISM330DHCX_SPI                      hspi2
#define HANDLE_ACC_SPI                             SPI2
#define MX_ACC_SPI_INIT                            MX_SPI2_Init
#define ACC_SPI_DEINIT                             BSP_SPI2_DeInit

/* ST25DV nfc Device */
#define BSP_ST25DV_I2C_INIT                     BSP_I2C2_Init
#define BSP_ST25DV_I2C_DEINIT                   BSP_I2C2_DeInit
#define BSP_ST25DV_I2C_READ_REG_16              BSP_I2C2_ReadReg16
#define BSP_ST25DV_I2C_WRITE_REG_16             BSP_I2C2_WriteReg16
#define BSP_ST25DV_I2C_RECV                     BSP_I2C2_Recv
#define BSP_ST25DV_I2C_IS_READY                 BSP_I2C2_IsReady

/* nfctag GPO pin */
extern EXTI_HandleTypeDef                       hexti13;
#define H_EXTI_13                               hexti13
#define GPO_EXTI                                hexti13
#define BSP_GPO_PIN                             GPIO_PIN_13
#define BSP_GPO_GPIO_PORT                       GPIOB
#define BSP_GPO_EXTI_LINE                       EXTI_LINE_13
#define BSP_GPO_EXTI_IRQN                       EXTI13_IRQn
#define BSP_GPO_CLK_ENABLE()                    __HAL_RCC_GPIOB_CLK_ENABLE()
#define BSP_GPO_EXTI_IRQHANDLER                 EXTI13_IRQHandler

/* Analog and digital mics */
#define ONBOARD_ANALOG_MIC          1
#define ONBOARD_DIGITAL_MIC         1

/* BSP COM Port */
#define BSP_COM_BAUDRATE      115200

/* SD card interrupt priority */
#define BSP_SD_IT_PRIORITY            14U  /* Default is lowest priority level */
#define BSP_SD_RX_IT_PRIORITY         14U  /* Default is lowest priority level */
#define BSP_SD_TX_IT_PRIORITY         15U  /* Default is lowest priority level */

/**  Definition for SD DETECT INTERRUPT PIN  **/
#define BSP_SD_DETECT_PIN                       GPIO_PIN_1
#define BSP_SD_DETECT_GPIO_PORT                 GPIOG
#define BSP_SD_DETECT_GPIO_CLK_ENABLE()         __HAL_RCC_GPIOG_CLK_ENABLE()
#define BSP_SD_DETECT_GPIO_CLK_DISABLE()        __HAL_RCC_GPIOG_CLK_DISABLE()
#define BSP_SD_DETECT_EXTI_LINE                 EXTI_LINE_1
#define BSP_SD_DETECT_EXTI_IRQN                 EXTI1_IRQn
#define BSP_SD_DETECT_IRQHANDLER                EXTI1_IRQHandler

#define BUTTON_USER_IT_PRIORITY   14U
#define BUTTON_PWR_IT_PRIORITY    14U

/* Define 1 to use already implemented callback; 0 to implement callback
   into an application file */

#define USE_MOTION_SENSOR_IIS2DLPC_0    0U
#define USE_MOTION_SENSOR_IIS2MDC_0     1U
#define USE_MOTION_SENSOR_IIS3DWB_0     0U
#define USE_MOTION_SENSOR_ISM330DHCX_0  1U
#define USE_MOTION_SENSOR_IIS2ICLX_0    0U

#define USE_ENV_SENSOR_ILPS22QS_0       1U
#define USE_ENV_SENSOR_STTS22H_0        1U

#define BSP_NFCTAG_INSTANCE             0U

#if (USE_MOTION_SENSOR_IIS2DLPC_0 + USE_MOTION_SENSOR_ISM330DHCX_0 + USE_MOTION_SENSOR_IIS2MDC_0 + USE_MOTION_SENSOR_IIS3DWB_0 + USE_MOTION_SENSOR_IIS2ICLX_0 == 0)
#undef USE_MOTION_SENSOR_ISM330DHCX_0
#define USE_MOTION_SENSOR_ISM330DHCX_0     1U
#endif /* USE_MOTION_SENSOR_ISM330DHCX_0 */

#if (USE_ENV_SENSOR_STTS22H_0 + USE_ENV_SENSOR_ILPS22QS_0 == 0)
#undef USE_ENV_SENSOR_STTS22H_0
#define USE_ENV_SENSOR_STTS22H_0     1U
#endif /* (USE_ENV_SENSOR_STTS22H_0 + USE_ENV_SENSOR_ILPS22QS_0 == 0) */

#ifdef __cplusplus
}
#endif

#endif /* STWIN_BOX_CONF_H__*/
