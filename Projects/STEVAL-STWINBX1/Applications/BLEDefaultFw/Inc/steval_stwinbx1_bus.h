/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : steval_stwinbx1_bus.h
  * @brief          : header file for the BSP BUS IO driver
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
#ifndef STEVAL_STWINBX1_BUS_H
#define STEVAL_STWINBX1_BUS_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "steval_stwinbx1_conf.h"
#include "steval_stwinbx1_errno.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STEVAL_STWINBX1
  * @{
  */

/** @defgroup STEVAL_STWINBX1_BUS STEVAL_STWINBX1 BUS
  * @{
  */

/** @defgroup STEVAL_STWINBX1_BUS_Exported_Constants STEVAL_STWINBX1 BUS Exported Constants
  * @{
  */

#define BUS_SPI2_INSTANCE SPI2
#define BUS_SPI2_SCK_GPIO_PORT GPIOI
#define BUS_SPI2_SCK_GPIO_PIN GPIO_PIN_1
#define BUS_SPI2_SCK_GPIO_AF GPIO_AF5_SPI2
#define BUS_SPI2_SCK_GPIO_CLK_DISABLE() __HAL_RCC_GPIOI_CLK_DISABLE()
#define BUS_SPI2_SCK_GPIO_CLK_ENABLE() __HAL_RCC_GPIOI_CLK_ENABLE()
#define BUS_SPI2_MISO_GPIO_CLK_DISABLE() __HAL_RCC_GPIOD_CLK_DISABLE()
#define BUS_SPI2_MISO_GPIO_PORT GPIOD
#define BUS_SPI2_MISO_GPIO_AF GPIO_AF5_SPI2
#define BUS_SPI2_MISO_GPIO_CLK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#define BUS_SPI2_MISO_GPIO_PIN GPIO_PIN_3
#define BUS_SPI2_MOSI_GPIO_CLK_ENABLE() __HAL_RCC_GPIOI_CLK_ENABLE()
#define BUS_SPI2_MOSI_GPIO_CLK_DISABLE() __HAL_RCC_GPIOI_CLK_DISABLE()
#define BUS_SPI2_MOSI_GPIO_AF GPIO_AF5_SPI2
#define BUS_SPI2_MOSI_GPIO_PORT GPIOI
#define BUS_SPI2_MOSI_GPIO_PIN GPIO_PIN_3

#ifndef BUS_SPI2_POLL_TIMEOUT
  #define BUS_SPI2_POLL_TIMEOUT                   0x1000U
#endif
/* SPI2 Baud rate in bps  */
#ifndef BUS_SPI2_BAUDRATE
   #define BUS_SPI2_BAUDRATE   10000000U /* baud rate of SPIn = 10 Mbps*/
#endif

#define BUS_I2C2_INSTANCE I2C2
#define BUS_I2C2_SDA_GPIO_CLK_DISABLE() __HAL_RCC_GPIOF_CLK_DISABLE()
#define BUS_I2C2_SDA_GPIO_CLK_ENABLE() __HAL_RCC_GPIOF_CLK_ENABLE()
#define BUS_I2C2_SDA_GPIO_PORT GPIOF
#define BUS_I2C2_SDA_GPIO_PIN GPIO_PIN_0
#define BUS_I2C2_SDA_GPIO_AF GPIO_AF4_I2C2
#define BUS_I2C2_SCL_GPIO_AF GPIO_AF4_I2C2
#define BUS_I2C2_SCL_GPIO_PORT GPIOH
#define BUS_I2C2_SCL_GPIO_CLK_DISABLE() __HAL_RCC_GPIOH_CLK_DISABLE()
#define BUS_I2C2_SCL_GPIO_PIN GPIO_PIN_4
#define BUS_I2C2_SCL_GPIO_CLK_ENABLE() __HAL_RCC_GPIOH_CLK_ENABLE()

#ifndef BUS_I2C2_POLL_TIMEOUT
   #define BUS_I2C2_POLL_TIMEOUT                0x1000U
#endif
/* I2C2 Frequency in Hz  */
#ifndef BUS_I2C2_FREQUENCY
   #define BUS_I2C2_FREQUENCY  1000000U /* Frequency of I2Cn = 100 KHz*/
#endif

#define BUS_SPI3_INSTANCE SPI3
#define BUS_SPI3_SCK_GPIO_PORT GPIOG
#define BUS_SPI3_SCK_GPIO_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()
#define BUS_SPI3_SCK_GPIO_CLK_DISABLE() __HAL_RCC_GPIOG_CLK_DISABLE()
#define BUS_SPI3_SCK_GPIO_PIN GPIO_PIN_9
#define BUS_SPI3_SCK_GPIO_AF GPIO_AF6_SPI3
#define BUS_SPI3_NSS_GPIO_PIN GPIO_PIN_15
#define BUS_SPI3_NSS_GPIO_AF GPIO_AF6_SPI3
#define BUS_SPI3_NSS_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()
#define BUS_SPI3_NSS_GPIO_CLK_DISABLE() __HAL_RCC_GPIOA_CLK_DISABLE()
#define BUS_SPI3_NSS_GPIO_PORT GPIOA
#define BUS_SPI3_MISO_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define BUS_SPI3_MISO_GPIO_PIN GPIO_PIN_4
#define BUS_SPI3_MISO_GPIO_PORT GPIOB
#define BUS_SPI3_MISO_GPIO_CLK_DISABLE() __HAL_RCC_GPIOB_CLK_DISABLE()
#define BUS_SPI3_MISO_GPIO_AF GPIO_AF6_SPI3
#define BUS_SPI3_MOSI_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define BUS_SPI3_MOSI_GPIO_AF GPIO_AF6_SPI3
#define BUS_SPI3_MOSI_GPIO_PORT GPIOB
#define BUS_SPI3_MOSI_GPIO_PIN GPIO_PIN_5
#define BUS_SPI3_MOSI_GPIO_CLK_DISABLE() __HAL_RCC_GPIOB_CLK_DISABLE()

#ifndef BUS_SPI3_POLL_TIMEOUT
  #define BUS_SPI3_POLL_TIMEOUT                   0x1000U
#endif
/* SPI3 Baud rate in bps  */
#ifndef BUS_SPI3_BAUDRATE
   #define BUS_SPI3_BAUDRATE   10000000U /* baud rate of SPIn = 10 Mbps*/
#endif

/**
  * @}
  */

/** @defgroup STEVAL_STWINBX1_BUS_Private_Types STEVAL_STWINBX1 BUS Private types
  * @{
  */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
typedef struct
{
  pI2C_CallbackTypeDef  pMspInitCb;
  pI2C_CallbackTypeDef  pMspDeInitCb;
}BSP_I2C_Cb_t;
#endif /* (USE_HAL_I2C_REGISTER_CALLBACKS == 1U) */
#if (USE_HAL_SPI_REGISTER_CALLBACKS == 1U)
typedef struct
{
  pSPI_CallbackTypeDef  pMspInitCb;
  pSPI_CallbackTypeDef  pMspDeInitCb;
}BSP_SPI_Cb_t;
#endif /* (USE_HAL_SPI_REGISTER_CALLBACKS == 1U) */
/**
  * @}
  */

/** @defgroup STEVAL_STWINBX1_LOW_LEVEL_Exported_Variables LOW LEVEL Exported Constants
  * @{
  */

extern SPI_HandleTypeDef hspi2;
extern I2C_HandleTypeDef hi2c2;
extern SPI_HandleTypeDef hspi3;

/**
  * @}
  */

/** @addtogroup STEVAL_STWINBX1_BUS_Exported_Functions
  * @{
  */

/* BUS IO driver over SPI Peripheral */
HAL_StatusTypeDef MX_SPI2_Init(SPI_HandleTypeDef* hspi);
int32_t BSP_SPI2_Init(void);
int32_t BSP_SPI2_DeInit(void);
int32_t BSP_SPI2_Send(uint8_t *pData, uint16_t Length);
int32_t BSP_SPI2_Recv(uint8_t *pData, uint16_t Length);
int32_t BSP_SPI2_SendRecv(uint8_t *pTxData, uint8_t *pRxData, uint16_t Length);
#if (USE_HAL_SPI_REGISTER_CALLBACKS == 1U)
int32_t BSP_SPI2_RegisterDefaultMspCallbacks (void);
int32_t BSP_SPI2_RegisterMspCallbacks (BSP_SPI_Cb_t *Callbacks);
#endif /* (USE_HAL_SPI_REGISTER_CALLBACKS == 1U) */

/* BUS IO driver over I2C Peripheral */
HAL_StatusTypeDef MX_I2C2_Init(I2C_HandleTypeDef* hi2c);
int32_t BSP_I2C2_Init(void);
int32_t BSP_I2C2_DeInit(void);
int32_t BSP_I2C2_IsReady(uint16_t DevAddr, uint32_t Trials);
int32_t BSP_I2C2_WriteReg(uint16_t Addr, uint16_t Reg, uint8_t *pData, uint16_t Length);
int32_t BSP_I2C2_ReadReg(uint16_t Addr, uint16_t Reg, uint8_t *pData, uint16_t Length);
int32_t BSP_I2C2_WriteReg16(uint16_t Addr, uint16_t Reg, uint8_t *pData, uint16_t Length);
int32_t BSP_I2C2_ReadReg16(uint16_t Addr, uint16_t Reg, uint8_t *pData, uint16_t Length);
int32_t BSP_I2C2_Send(uint16_t DevAddr, uint8_t *pData, uint16_t Length);
int32_t BSP_I2C2_Recv(uint16_t DevAddr, uint8_t *pData, uint16_t Length);
int32_t BSP_I2C2_SendRecv(uint16_t DevAddr, uint8_t *pTxdata, uint8_t *pRxdata, uint16_t Length);
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
int32_t BSP_I2C2_RegisterDefaultMspCallbacks (void);
int32_t BSP_I2C2_RegisterMspCallbacks (BSP_I2C_Cb_t *Callbacks);
#endif /* (USE_HAL_I2C_REGISTER_CALLBACKS == 1U) */
HAL_StatusTypeDef MX_SPI3_Init(SPI_HandleTypeDef* hspi);
int32_t BSP_SPI3_Init(void);
int32_t BSP_SPI3_DeInit(void);
int32_t BSP_SPI3_Send(uint8_t *pData, uint16_t Length);
int32_t BSP_SPI3_Recv(uint8_t *pData, uint16_t Length);
int32_t BSP_SPI3_SendRecv(uint8_t *pTxData, uint8_t *pRxData, uint16_t Length);
#if (USE_HAL_SPI_REGISTER_CALLBACKS == 1U)
int32_t BSP_SPI3_RegisterDefaultMspCallbacks (void);
int32_t BSP_SPI3_RegisterMspCallbacks (BSP_SPI_Cb_t *Callbacks);
#endif /* (USE_HAL_SPI_REGISTER_CALLBACKS == 1U) */

int32_t BSP_GetTick(void);

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

#endif /* STEVAL_STWINBX1_BUS_H */

