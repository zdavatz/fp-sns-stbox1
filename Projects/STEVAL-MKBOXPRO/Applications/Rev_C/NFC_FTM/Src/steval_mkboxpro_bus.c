/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : steval_mkboxpro_bus.c
  * @brief          : source file for the BSP BUS IO driver
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

/* Includes ------------------------------------------------------------------*/
#include "steval_mkboxpro_bus.h"

__weak HAL_StatusTypeDef MX_I2C2_Init(I2C_HandleTypeDef* hi2c);
__weak HAL_StatusTypeDef MX_I2C4_Init(I2C_HandleTypeDef* hi2c);
__weak HAL_StatusTypeDef MX_I2C3_Init(I2C_HandleTypeDef* hi2c);
__weak HAL_StatusTypeDef MX_I2C1_Init(I2C_HandleTypeDef* hi2c);

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STEVAL_MKBOXPRO
  * @{
  */

/** @defgroup STEVAL_MKBOXPRO_BUS STEVAL_MKBOXPRO BUS
  * @{
  */

/** @defgroup STEVAL_MKBOXPRO_BUS_Exported_Variables BUS Exported Variables
  * @{
  */

I2C_HandleTypeDef hi2c2;
I2C_HandleTypeDef hi2c4;
I2C_HandleTypeDef hi2c3;
I2C_HandleTypeDef hi2c1;
/**
  * @}
  */

/** @defgroup STEVAL_MKBOXPRO_BUS_Private_Variables BUS Private Variables
  * @{
  */

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
static uint32_t IsI2C2MspCbValid = 0;
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
static uint32_t I2C2InitCounter = 0;
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
static uint32_t IsI2C4MspCbValid = 0;
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
static uint32_t I2C4InitCounter = 0;
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
static uint32_t IsI2C3MspCbValid = 0;
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
static uint32_t I2C3InitCounter = 0;
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
static uint32_t IsI2C1MspCbValid = 0;
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
static uint32_t I2C1InitCounter = 0;

/**
  * @}
  */

/** @defgroup STEVAL_MKBOXPRO_BUS_Private_FunctionPrototypes  BUS Private Function
  * @{
  */

static void I2C2_MspInit(I2C_HandleTypeDef* hI2c);
static void I2C2_MspDeInit(I2C_HandleTypeDef* hI2c);
#if (USE_CUBEMX_BSP_V2 == 1)
static uint32_t I2C_GetTiming(uint32_t clock_src_hz, uint32_t i2cfreq_hz);
static void Compute_PRESC_SCLDEL_SDADEL(uint32_t clock_src_freq, uint32_t I2C_Speed);
static uint32_t Compute_SCLL_SCLH (uint32_t clock_src_freq, uint32_t I2C_speed);
#endif
static void I2C4_MspInit(I2C_HandleTypeDef* hI2c);
static void I2C4_MspDeInit(I2C_HandleTypeDef* hI2c);
static void I2C3_MspInit(I2C_HandleTypeDef* hI2c);
static void I2C3_MspDeInit(I2C_HandleTypeDef* hI2c);
static void I2C1_MspInit(I2C_HandleTypeDef* hI2c);
static void I2C1_MspDeInit(I2C_HandleTypeDef* hI2c);

/**
  * @}
  */

/** @defgroup STEVAL_MKBOXPRO_LOW_LEVEL_Private_Functions STEVAL_MKBOXPRO LOW LEVEL Private Functions
  * @{
  */

/** @defgroup STEVAL_MKBOXPRO_BUS_Exported_Functions STEVAL_MKBOXPRO_BUS Exported Functions
  * @{
  */

/* BUS IO driver over I2C Peripheral */
/*******************************************************************************
                            BUS OPERATIONS OVER I2C
*******************************************************************************/
/**
  * @brief  Initialize I2C HAL
  * @retval BSP status
  */
int32_t BSP_I2C2_Init(void)
{

  int32_t ret = BSP_ERROR_NONE;

  hi2c2.Instance  = I2C2;

  if(I2C2InitCounter++ == 0)
  {
    if (HAL_I2C_GetState(&hi2c2) == HAL_I2C_STATE_RESET)
    {
    #if (USE_HAL_I2C_REGISTER_CALLBACKS == 0U)
      /* Init the I2C Msp */
      I2C2_MspInit(&hi2c2);
    #else
      if(IsI2C2MspCbValid == 0U)
      {
        if(BSP_I2C2_RegisterDefaultMspCallbacks() != BSP_ERROR_NONE)
        {
          return BSP_ERROR_MSP_FAILURE;
        }
      }
    #endif
      if(ret == BSP_ERROR_NONE)
      {
        /* Init the I2C */
        if(MX_I2C2_Init(&hi2c2) != HAL_OK)
        {
          ret = BSP_ERROR_BUS_FAILURE;
        }
        else if(HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        {
          ret = BSP_ERROR_BUS_FAILURE;
        }
        else
        {
          ret = BSP_ERROR_NONE;
        }
      }
    }
  }
  return ret;
}

/**
  * @brief  DeInitialize I2C HAL.
  * @retval BSP status
  */
int32_t BSP_I2C2_DeInit(void)
{
  int32_t ret = BSP_ERROR_NONE;

  if (I2C2InitCounter > 0)
  {
    if (--I2C2InitCounter == 0)
    {
  #if (USE_HAL_I2C_REGISTER_CALLBACKS == 0U)
      /* DeInit the I2C */
      I2C2_MspDeInit(&hi2c2);
  #endif
      /* DeInit the I2C */
      if (HAL_I2C_DeInit(&hi2c2) != HAL_OK)
      {
        ret = BSP_ERROR_BUS_FAILURE;
      }
    }
  }
  return ret;
}

/**
  * @brief  Check whether the I2C bus is ready.
  * @param DevAddr : I2C device address
  * @param Trials : Check trials number
  * @retval BSP status
  */
int32_t BSP_I2C2_IsReady(uint16_t DevAddr, uint32_t Trials)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_IsDeviceReady(&hi2c2, DevAddr, Trials, BUS_I2C2_POLL_TIMEOUT) != HAL_OK)
  {
    ret = BSP_ERROR_BUSY;
  }

  return ret;
}

/**
  * @brief  Write a value in a register of the device through BUS.
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to write
  * @param  pData  Pointer to data buffer to write
  * @param  Length Data Length
  * @retval BSP status
  */

int32_t BSP_I2C2_WriteReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Write(&hi2c2, DevAddr,Reg, I2C_MEMADD_SIZE_8BIT,pData, Length, BUS_I2C2_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c2) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Read a register of the device through BUS
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to read
  * @param  pData  Pointer to data buffer to read
  * @param  Length Data Length
  * @retval BSP status
  */
int32_t  BSP_I2C2_ReadReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Read(&hi2c2, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, BUS_I2C2_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c2) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**

  * @brief  Write a value in a register of the device through BUS.
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to write

  * @param  pData  Pointer to data buffer to write
  * @param  Length Data Length
  * @retval BSP statu
  */
int32_t BSP_I2C2_WriteReg16(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Write(&hi2c2, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length, BUS_I2C2_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c2) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Read registers through a bus (16 bits)
  * @param  DevAddr: Device address on BUS
  * @param  Reg: The target register address to read
  * @param  Length Data Length
  * @retval BSP status
  */
int32_t  BSP_I2C2_ReadReg16(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Read(&hi2c2, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length, BUS_I2C2_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c2) != HAL_I2C_ERROR_AF)
    {
      ret =  BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Send an amount width data through bus (Simplex)
  * @param  DevAddr: Device address on Bus.
  * @param  pData: Data pointer
  * @param  Length: Data length
  * @retval BSP status
  */
int32_t BSP_I2C2_Send(uint16_t DevAddr, uint8_t *pData, uint16_t Length) {
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Master_Transmit(&hi2c2, DevAddr, pData, Length, BUS_I2C2_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c2) != HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }

  return ret;
}

/**
  * @brief  Receive an amount of data through a bus (Simplex)
  * @param  DevAddr: Device address on Bus.
  * @param  pData: Data pointer
  * @param  Length: Data length
  * @retval BSP status
  */
int32_t BSP_I2C2_Recv(uint16_t DevAddr, uint8_t *pData, uint16_t Length) {
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Master_Receive(&hi2c2, DevAddr, pData, Length, BUS_I2C2_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c2) != HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/*******************************************************************************
                            BUS OPERATIONS OVER I2C
*******************************************************************************/
/**
  * @brief  Initialize I2C HAL
  * @retval BSP status
  */
int32_t BSP_I2C4_Init(void)
{

  int32_t ret = BSP_ERROR_NONE;

  hi2c4.Instance  = I2C4;

  if(I2C4InitCounter++ == 0)
  {
    if (HAL_I2C_GetState(&hi2c4) == HAL_I2C_STATE_RESET)
    {
    #if (USE_HAL_I2C_REGISTER_CALLBACKS == 0U)
      /* Init the I2C Msp */
      I2C4_MspInit(&hi2c4);
    #else
      if(IsI2C4MspCbValid == 0U)
      {
        if(BSP_I2C4_RegisterDefaultMspCallbacks() != BSP_ERROR_NONE)
        {
          return BSP_ERROR_MSP_FAILURE;
        }
      }
    #endif
      if(ret == BSP_ERROR_NONE)
      {
        /* Init the I2C */
        if(MX_I2C4_Init(&hi2c4) != HAL_OK)
        {
          ret = BSP_ERROR_BUS_FAILURE;
        }
        else if(HAL_I2CEx_ConfigAnalogFilter(&hi2c4, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        {
          ret = BSP_ERROR_BUS_FAILURE;
        }
        else
        {
          ret = BSP_ERROR_NONE;
        }
      }
    }
  }
  return ret;
}

/**
  * @brief  DeInitialize I2C HAL.
  * @retval BSP status
  */
int32_t BSP_I2C4_DeInit(void)
{
  int32_t ret = BSP_ERROR_NONE;

  if (I2C4InitCounter > 0)
  {
    if (--I2C4InitCounter == 0)
    {
  #if (USE_HAL_I2C_REGISTER_CALLBACKS == 0U)
      /* DeInit the I2C */
      I2C4_MspDeInit(&hi2c4);
  #endif
      /* DeInit the I2C */
      if (HAL_I2C_DeInit(&hi2c4) != HAL_OK)
      {
        ret = BSP_ERROR_BUS_FAILURE;
      }
    }
  }
  return ret;
}

/**
  * @brief  Check whether the I2C bus is ready.
  * @param DevAddr : I2C device address
  * @param Trials : Check trials number
  * @retval BSP status
  */
int32_t BSP_I2C4_IsReady(uint16_t DevAddr, uint32_t Trials)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_IsDeviceReady(&hi2c4, DevAddr, Trials, BUS_I2C4_POLL_TIMEOUT) != HAL_OK)
  {
    ret = BSP_ERROR_BUSY;
  }

  return ret;
}

/**
  * @brief  Write a value in a register of the device through BUS.
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to write
  * @param  pData  Pointer to data buffer to write
  * @param  Length Data Length
  * @retval BSP status
  */

int32_t BSP_I2C4_WriteReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Write(&hi2c4, DevAddr,Reg, I2C_MEMADD_SIZE_8BIT,pData, Length, BUS_I2C4_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c4) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Read a register of the device through BUS
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to read
  * @param  pData  Pointer to data buffer to read
  * @param  Length Data Length
  * @retval BSP status
  */
int32_t  BSP_I2C4_ReadReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Read(&hi2c4, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, BUS_I2C4_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c4) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**

  * @brief  Write a value in a register of the device through BUS.
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to write

  * @param  pData  Pointer to data buffer to write
  * @param  Length Data Length
  * @retval BSP statu
  */
int32_t BSP_I2C4_WriteReg16(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Write(&hi2c4, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length, BUS_I2C4_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c4) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Read registers through a bus (16 bits)
  * @param  DevAddr: Device address on BUS
  * @param  Reg: The target register address to read
  * @param  Length Data Length
  * @retval BSP status
  */
int32_t  BSP_I2C4_ReadReg16(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Read(&hi2c4, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length, BUS_I2C4_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c4) != HAL_I2C_ERROR_AF)
    {
      ret =  BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Send an amount width data through bus (Simplex)
  * @param  DevAddr: Device address on Bus.
  * @param  pData: Data pointer
  * @param  Length: Data length
  * @retval BSP status
  */
int32_t BSP_I2C4_Send(uint16_t DevAddr, uint8_t *pData, uint16_t Length) {
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Master_Transmit(&hi2c4, DevAddr, pData, Length, BUS_I2C4_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c4) != HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }

  return ret;
}

/**
  * @brief  Receive an amount of data through a bus (Simplex)
  * @param  DevAddr: Device address on Bus.
  * @param  pData: Data pointer
  * @param  Length: Data length
  * @retval BSP status
  */
int32_t BSP_I2C4_Recv(uint16_t DevAddr, uint8_t *pData, uint16_t Length) {
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Master_Receive(&hi2c4, DevAddr, pData, Length, BUS_I2C4_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c4) != HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/*******************************************************************************
                            BUS OPERATIONS OVER I2C
*******************************************************************************/
/**
  * @brief  Initialize I2C HAL
  * @retval BSP status
  */
int32_t BSP_I2C3_Init(void)
{

  int32_t ret = BSP_ERROR_NONE;

  hi2c3.Instance  = I2C3;

  if(I2C3InitCounter++ == 0)
  {
    if (HAL_I2C_GetState(&hi2c3) == HAL_I2C_STATE_RESET)
    {
    #if (USE_HAL_I2C_REGISTER_CALLBACKS == 0U)
      /* Init the I2C Msp */
      I2C3_MspInit(&hi2c3);
    #else
      if(IsI2C3MspCbValid == 0U)
      {
        if(BSP_I2C3_RegisterDefaultMspCallbacks() != BSP_ERROR_NONE)
        {
          return BSP_ERROR_MSP_FAILURE;
        }
      }
    #endif
      if(ret == BSP_ERROR_NONE)
      {
        /* Init the I2C */
        if(MX_I2C3_Init(&hi2c3) != HAL_OK)
        {
          ret = BSP_ERROR_BUS_FAILURE;
        }
        else if(HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        {
          ret = BSP_ERROR_BUS_FAILURE;
        }
        else
        {
          ret = BSP_ERROR_NONE;
        }
      }
    }
  }
  return ret;
}

/**
  * @brief  DeInitialize I2C HAL.
  * @retval BSP status
  */
int32_t BSP_I2C3_DeInit(void)
{
  int32_t ret = BSP_ERROR_NONE;

  if (I2C3InitCounter > 0)
  {
    if (--I2C3InitCounter == 0)
    {
  #if (USE_HAL_I2C_REGISTER_CALLBACKS == 0U)
      /* DeInit the I2C */
      I2C3_MspDeInit(&hi2c3);
  #endif
      /* DeInit the I2C */
      if (HAL_I2C_DeInit(&hi2c3) != HAL_OK)
      {
        ret = BSP_ERROR_BUS_FAILURE;
      }
    }
  }
  return ret;
}

/**
  * @brief  Check whether the I2C bus is ready.
  * @param DevAddr : I2C device address
  * @param Trials : Check trials number
  * @retval BSP status
  */
int32_t BSP_I2C3_IsReady(uint16_t DevAddr, uint32_t Trials)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_IsDeviceReady(&hi2c3, DevAddr, Trials, BUS_I2C3_POLL_TIMEOUT) != HAL_OK)
  {
    ret = BSP_ERROR_BUSY;
  }

  return ret;
}

/**
  * @brief  Write a value in a register of the device through BUS.
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to write
  * @param  pData  Pointer to data buffer to write
  * @param  Length Data Length
  * @retval BSP status
  */

int32_t BSP_I2C3_WriteReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Write(&hi2c3, DevAddr,Reg, I2C_MEMADD_SIZE_8BIT,pData, Length, BUS_I2C3_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c3) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Read a register of the device through BUS
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to read
  * @param  pData  Pointer to data buffer to read
  * @param  Length Data Length
  * @retval BSP status
  */
int32_t  BSP_I2C3_ReadReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Read(&hi2c3, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, BUS_I2C3_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c3) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**

  * @brief  Write a value in a register of the device through BUS.
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to write

  * @param  pData  Pointer to data buffer to write
  * @param  Length Data Length
  * @retval BSP statu
  */
int32_t BSP_I2C3_WriteReg16(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Write(&hi2c3, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length, BUS_I2C3_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c3) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Read registers through a bus (16 bits)
  * @param  DevAddr: Device address on BUS
  * @param  Reg: The target register address to read
  * @param  Length Data Length
  * @retval BSP status
  */
int32_t  BSP_I2C3_ReadReg16(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Read(&hi2c3, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length, BUS_I2C3_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c3) != HAL_I2C_ERROR_AF)
    {
      ret =  BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Send an amount width data through bus (Simplex)
  * @param  DevAddr: Device address on Bus.
  * @param  pData: Data pointer
  * @param  Length: Data length
  * @retval BSP status
  */
int32_t BSP_I2C3_Send(uint16_t DevAddr, uint8_t *pData, uint16_t Length) {
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Master_Transmit(&hi2c3, DevAddr, pData, Length, BUS_I2C3_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c3) != HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }

  return ret;
}

/**
  * @brief  Receive an amount of data through a bus (Simplex)
  * @param  DevAddr: Device address on Bus.
  * @param  pData: Data pointer
  * @param  Length: Data length
  * @retval BSP status
  */
int32_t BSP_I2C3_Recv(uint16_t DevAddr, uint8_t *pData, uint16_t Length) {
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Master_Receive(&hi2c3, DevAddr, pData, Length, BUS_I2C3_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c3) != HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/*******************************************************************************
                            BUS OPERATIONS OVER I2C
*******************************************************************************/
/**
  * @brief  Initialize I2C HAL
  * @retval BSP status
  */
int32_t BSP_I2C1_Init(void)
{

  int32_t ret = BSP_ERROR_NONE;

  hi2c1.Instance  = I2C1;

  if(I2C1InitCounter++ == 0)
  {
    if (HAL_I2C_GetState(&hi2c1) == HAL_I2C_STATE_RESET)
    {
    #if (USE_HAL_I2C_REGISTER_CALLBACKS == 0U)
      /* Init the I2C Msp */
      I2C1_MspInit(&hi2c1);
    #else
      if(IsI2C1MspCbValid == 0U)
      {
        if(BSP_I2C1_RegisterDefaultMspCallbacks() != BSP_ERROR_NONE)
        {
          return BSP_ERROR_MSP_FAILURE;
        }
      }
    #endif
      if(ret == BSP_ERROR_NONE)
      {
        /* Init the I2C */
        if(MX_I2C1_Init(&hi2c1) != HAL_OK)
        {
          ret = BSP_ERROR_BUS_FAILURE;
        }
        else if(HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
        {
          ret = BSP_ERROR_BUS_FAILURE;
        }
        else
        {
          ret = BSP_ERROR_NONE;
        }
      }
    }
  }
  return ret;
}

/**
  * @brief  DeInitialize I2C HAL.
  * @retval BSP status
  */
int32_t BSP_I2C1_DeInit(void)
{
  int32_t ret = BSP_ERROR_NONE;

  if (I2C1InitCounter > 0)
  {
    if (--I2C1InitCounter == 0)
    {
  #if (USE_HAL_I2C_REGISTER_CALLBACKS == 0U)
      /* DeInit the I2C */
      I2C1_MspDeInit(&hi2c1);
  #endif
      /* DeInit the I2C */
      if (HAL_I2C_DeInit(&hi2c1) != HAL_OK)
      {
        ret = BSP_ERROR_BUS_FAILURE;
      }
    }
  }
  return ret;
}

/**
  * @brief  Check whether the I2C bus is ready.
  * @param DevAddr : I2C device address
  * @param Trials : Check trials number
  * @retval BSP status
  */
int32_t BSP_I2C1_IsReady(uint16_t DevAddr, uint32_t Trials)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_IsDeviceReady(&hi2c1, DevAddr, Trials, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    ret = BSP_ERROR_BUSY;
  }

  return ret;
}

/**
  * @brief  Write a value in a register of the device through BUS.
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to write
  * @param  pData  Pointer to data buffer to write
  * @param  Length Data Length
  * @retval BSP status
  */

int32_t BSP_I2C1_WriteReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Write(&hi2c1, DevAddr,Reg, I2C_MEMADD_SIZE_8BIT,pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Read a register of the device through BUS
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to read
  * @param  pData  Pointer to data buffer to read
  * @param  Length Data Length
  * @retval BSP status
  */
int32_t  BSP_I2C1_ReadReg(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Read(&hi2c1, DevAddr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret = BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**

  * @brief  Write a value in a register of the device through BUS.
  * @param  DevAddr Device address on Bus.
  * @param  Reg    The target register address to write

  * @param  pData  Pointer to data buffer to write
  * @param  Length Data Length
  * @retval BSP statu
  */
int32_t BSP_I2C1_WriteReg16(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Write(&hi2c1, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) == HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Read registers through a bus (16 bits)
  * @param  DevAddr: Device address on BUS
  * @param  Reg: The target register address to read
  * @param  Length Data Length
  * @retval BSP status
  */
int32_t  BSP_I2C1_ReadReg16(uint16_t DevAddr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Mem_Read(&hi2c1, DevAddr, Reg, I2C_MEMADD_SIZE_16BIT, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) != HAL_I2C_ERROR_AF)
    {
      ret =  BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Send an amount width data through bus (Simplex)
  * @param  DevAddr: Device address on Bus.
  * @param  pData: Data pointer
  * @param  Length: Data length
  * @retval BSP status
  */
int32_t BSP_I2C1_Send(uint16_t DevAddr, uint8_t *pData, uint16_t Length) {
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Master_Transmit(&hi2c1, DevAddr, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) != HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }

  return ret;
}

/**
  * @brief  Receive an amount of data through a bus (Simplex)
  * @param  DevAddr: Device address on Bus.
  * @param  pData: Data pointer
  * @param  Length: Data length
  * @retval BSP status
  */
int32_t BSP_I2C1_Recv(uint16_t DevAddr, uint8_t *pData, uint16_t Length) {
  int32_t ret = BSP_ERROR_NONE;

  if (HAL_I2C_Master_Receive(&hi2c1, DevAddr, pData, Length, BUS_I2C1_POLL_TIMEOUT) != HAL_OK)
  {
    if (HAL_I2C_GetError(&hi2c1) != HAL_I2C_ERROR_AF)
    {
      ret = BSP_ERROR_BUS_ACKNOWLEDGE_FAILURE;
    }
    else
    {
      ret =  BSP_ERROR_PERIPH_FAILURE;
    }
  }
  return ret;
}

/**
  * @brief  Write Data through I2C BUS with DMA.
  * @param  pData: Pointer to data buffer to send
  * @param  Length: Length of data in byte
  * @retval BSP status
  */
int32_t BSP_I2C1_Send_DMA(uint16_t DevAddr, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if(HAL_I2C_Master_Transmit_DMA(&hi2c1, DevAddr, pData, Length) != HAL_OK)
  {
      ret = BSP_ERROR_UNKNOWN_FAILURE;
  }
  return ret;
}
/**
  * @brief  Receive Data from I2C BUS with DMA
  * @param  pData: Pointer to data buffer to receive
  * @param  Length: Length of data in byte
  * @retval BSP status
  */
int32_t  BSP_I2C1_Recv_DMA(uint16_t DevAddr, uint8_t *pData, uint16_t Length)
{
  int32_t ret = BSP_ERROR_NONE;

  if(HAL_I2C_Master_Receive_DMA(&hi2c1, DevAddr, pData, Length) != HAL_OK)
  {
      ret = BSP_ERROR_UNKNOWN_FAILURE;
  }
  return ret;
}

#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
/**
  * @brief Register Default BSP I2C2 Bus Msp Callbacks
  * @retval BSP status
  */
int32_t BSP_I2C2_RegisterDefaultMspCallbacks (void)
{

  __HAL_I2C_RESET_HANDLE_STATE(&hi2c2);

  /* Register MspInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c2, HAL_I2C_MSPINIT_CB_ID, I2C2_MspInit)  != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  /* Register MspDeInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c2, HAL_I2C_MSPDEINIT_CB_ID, I2C2_MspDeInit) != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }
  IsI2C2MspCbValid = 1;

  return BSP_ERROR_NONE;
}

/**
  * @brief BSP I2C2 Bus Msp Callback registering
  * @param Callbacks     pointer to I2C2 MspInit/MspDeInit callback functions
  * @retval BSP status
  */
int32_t BSP_I2C2_RegisterMspCallbacks (BSP_I2C_Cb_t *Callbacks)
{
  /* Prevent unused argument(s) compilation warning */
  __HAL_I2C_RESET_HANDLE_STATE(&hi2c2);

   /* Register MspInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c2, HAL_I2C_MSPINIT_CB_ID, Callbacks->pMspInitCb)  != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  /* Register MspDeInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c2, HAL_I2C_MSPDEINIT_CB_ID, Callbacks->pMspDeInitCb) != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  IsI2C2MspCbValid = 1;

  return BSP_ERROR_NONE;
}
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
/**
  * @brief Register Default BSP I2C4 Bus Msp Callbacks
  * @retval BSP status
  */
int32_t BSP_I2C4_RegisterDefaultMspCallbacks (void)
{

  __HAL_I2C_RESET_HANDLE_STATE(&hi2c4);

  /* Register MspInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c4, HAL_I2C_MSPINIT_CB_ID, I2C4_MspInit)  != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  /* Register MspDeInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c4, HAL_I2C_MSPDEINIT_CB_ID, I2C4_MspDeInit) != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }
  IsI2C4MspCbValid = 1;

  return BSP_ERROR_NONE;
}

/**
  * @brief BSP I2C4 Bus Msp Callback registering
  * @param Callbacks     pointer to I2C4 MspInit/MspDeInit callback functions
  * @retval BSP status
  */
int32_t BSP_I2C4_RegisterMspCallbacks (BSP_I2C_Cb_t *Callbacks)
{
  /* Prevent unused argument(s) compilation warning */
  __HAL_I2C_RESET_HANDLE_STATE(&hi2c4);

   /* Register MspInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c4, HAL_I2C_MSPINIT_CB_ID, Callbacks->pMspInitCb)  != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  /* Register MspDeInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c4, HAL_I2C_MSPDEINIT_CB_ID, Callbacks->pMspDeInitCb) != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  IsI2C4MspCbValid = 1;

  return BSP_ERROR_NONE;
}
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
/**
  * @brief Register Default BSP I2C3 Bus Msp Callbacks
  * @retval BSP status
  */
int32_t BSP_I2C3_RegisterDefaultMspCallbacks (void)
{

  __HAL_I2C_RESET_HANDLE_STATE(&hi2c3);

  /* Register MspInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c3, HAL_I2C_MSPINIT_CB_ID, I2C3_MspInit)  != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  /* Register MspDeInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c3, HAL_I2C_MSPDEINIT_CB_ID, I2C3_MspDeInit) != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }
  IsI2C3MspCbValid = 1;

  return BSP_ERROR_NONE;
}

/**
  * @brief BSP I2C3 Bus Msp Callback registering
  * @param Callbacks     pointer to I2C3 MspInit/MspDeInit callback functions
  * @retval BSP status
  */
int32_t BSP_I2C3_RegisterMspCallbacks (BSP_I2C_Cb_t *Callbacks)
{
  /* Prevent unused argument(s) compilation warning */
  __HAL_I2C_RESET_HANDLE_STATE(&hi2c3);

   /* Register MspInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c3, HAL_I2C_MSPINIT_CB_ID, Callbacks->pMspInitCb)  != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  /* Register MspDeInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c3, HAL_I2C_MSPDEINIT_CB_ID, Callbacks->pMspDeInitCb) != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  IsI2C3MspCbValid = 1;

  return BSP_ERROR_NONE;
}
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */
#if (USE_HAL_I2C_REGISTER_CALLBACKS == 1U)
/**
  * @brief Register Default BSP I2C1 Bus Msp Callbacks
  * @retval BSP status
  */
int32_t BSP_I2C1_RegisterDefaultMspCallbacks (void)
{

  __HAL_I2C_RESET_HANDLE_STATE(&hi2c1);

  /* Register MspInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c1, HAL_I2C_MSPINIT_CB_ID, I2C1_MspInit)  != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  /* Register MspDeInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c1, HAL_I2C_MSPDEINIT_CB_ID, I2C1_MspDeInit) != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }
  IsI2C1MspCbValid = 1;

  return BSP_ERROR_NONE;
}

/**
  * @brief BSP I2C1 Bus Msp Callback registering
  * @param Callbacks     pointer to I2C1 MspInit/MspDeInit callback functions
  * @retval BSP status
  */
int32_t BSP_I2C1_RegisterMspCallbacks (BSP_I2C_Cb_t *Callbacks)
{
  /* Prevent unused argument(s) compilation warning */
  __HAL_I2C_RESET_HANDLE_STATE(&hi2c1);

   /* Register MspInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c1, HAL_I2C_MSPINIT_CB_ID, Callbacks->pMspInitCb)  != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  /* Register MspDeInit Callback */
  if (HAL_I2C_RegisterCallback(&hi2c1, HAL_I2C_MSPDEINIT_CB_ID, Callbacks->pMspDeInitCb) != HAL_OK)
  {
    return BSP_ERROR_PERIPH_FAILURE;
  }

  IsI2C1MspCbValid = 1;

  return BSP_ERROR_NONE;
}
#endif /* USE_HAL_I2C_REGISTER_CALLBACKS */

/**
  * @brief  Return system tick in ms
  * @retval Current HAL time base time stamp
  */
int32_t BSP_GetTick(void) {
  return HAL_GetTick();
}

/* I2C2 init function */

__weak HAL_StatusTypeDef MX_I2C2_Init(I2C_HandleTypeDef* hi2c)
{
  HAL_StatusTypeDef ret = HAL_OK;

  hi2c->Instance = I2C2;
  hi2c->Init.Timing = 0x00F07BFF;
  hi2c->Init.OwnAddress1 = 0;
  hi2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c->Init.OwnAddress2 = 0;
  hi2c->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(hi2c) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  if (HAL_I2CEx_ConfigAnalogFilter(hi2c, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  if (HAL_I2CEx_ConfigDigitalFilter(hi2c, 0) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  return ret;
}

static void I2C2_MspInit(I2C_HandleTypeDef* i2cHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  /* USER CODE BEGIN I2C2_MspInit 0 */

  /* USER CODE END I2C2_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
    PeriphClkInit.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C2 GPIO Configuration
    PB14     ------> I2C2_SDA
    PB13     ------> I2C2_SCL
    */
    GPIO_InitStruct.Pin = BUS_I2C2_SDA_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BUS_I2C2_SDA_GPIO_AF;
    HAL_GPIO_Init(BUS_I2C2_SDA_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BUS_I2C2_SCL_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BUS_I2C2_SCL_GPIO_AF;
    HAL_GPIO_Init(BUS_I2C2_SCL_GPIO_PORT, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_I2C2_CLK_ENABLE();

    /* Peripheral interrupt init */
    HAL_NVIC_SetPriority(I2C2_EV_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(I2C2_EV_IRQn);
    HAL_NVIC_SetPriority(I2C2_ER_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(I2C2_ER_IRQn);
  /* USER CODE BEGIN I2C2_MspInit 1 */

  /* USER CODE END I2C2_MspInit 1 */
}

static void I2C2_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{
  /* USER CODE BEGIN I2C2_MspDeInit 0 */

  /* USER CODE END I2C2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C2_CLK_DISABLE();

    /**I2C2 GPIO Configuration
    PB14     ------> I2C2_SDA
    PB13     ------> I2C2_SCL
    */
    HAL_GPIO_DeInit(BUS_I2C2_SDA_GPIO_PORT, BUS_I2C2_SDA_GPIO_PIN);

    HAL_GPIO_DeInit(BUS_I2C2_SCL_GPIO_PORT, BUS_I2C2_SCL_GPIO_PIN);

    /* Peripheral interrupt Deinit*/
    HAL_NVIC_DisableIRQ(I2C2_EV_IRQn);

    HAL_NVIC_DisableIRQ(I2C2_ER_IRQn);

  /* USER CODE BEGIN I2C2_MspDeInit 1 */

  /* USER CODE END I2C2_MspDeInit 1 */
}

/* I2C4 init function */

__weak HAL_StatusTypeDef MX_I2C4_Init(I2C_HandleTypeDef* hi2c)
{
  HAL_StatusTypeDef ret = HAL_OK;

  hi2c->Instance = I2C4;
  hi2c->Init.Timing = 0x00F07BFF;
  hi2c->Init.OwnAddress1 = 0;
  hi2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c->Init.OwnAddress2 = 0;
  hi2c->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(hi2c) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  if (HAL_I2CEx_ConfigAnalogFilter(hi2c, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  if (HAL_I2CEx_ConfigDigitalFilter(hi2c, 0) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  return ret;
}

static void I2C4_MspInit(I2C_HandleTypeDef* i2cHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  /* USER CODE BEGIN I2C4_MspInit 0 */

  /* USER CODE END I2C4_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C4;
    PeriphClkInit.I2c4ClockSelection = RCC_I2C4CLKSOURCE_PCLK1;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**I2C4 GPIO Configuration
    PD12     ------> I2C4_SCL
    PD13     ------> I2C4_SDA
    */
    GPIO_InitStruct.Pin = BUS_I2C4_SCL_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BUS_I2C4_SCL_GPIO_AF;
    HAL_GPIO_Init(BUS_I2C4_SCL_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BUS_I2C4_SDA_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BUS_I2C4_SDA_GPIO_AF;
    HAL_GPIO_Init(BUS_I2C4_SDA_GPIO_PORT, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_I2C4_CLK_ENABLE();

    /* Peripheral interrupt init */
    HAL_NVIC_SetPriority(I2C4_EV_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(I2C4_EV_IRQn);
    HAL_NVIC_SetPriority(I2C4_ER_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(I2C4_ER_IRQn);
  /* USER CODE BEGIN I2C4_MspInit 1 */

  /* USER CODE END I2C4_MspInit 1 */
}

static void I2C4_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{
  /* USER CODE BEGIN I2C4_MspDeInit 0 */

  /* USER CODE END I2C4_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C4_CLK_DISABLE();

    /**I2C4 GPIO Configuration
    PD12     ------> I2C4_SCL
    PD13     ------> I2C4_SDA
    */
    HAL_GPIO_DeInit(BUS_I2C4_SCL_GPIO_PORT, BUS_I2C4_SCL_GPIO_PIN);

    HAL_GPIO_DeInit(BUS_I2C4_SDA_GPIO_PORT, BUS_I2C4_SDA_GPIO_PIN);

    /* Peripheral interrupt Deinit*/
    HAL_NVIC_DisableIRQ(I2C4_EV_IRQn);

    HAL_NVIC_DisableIRQ(I2C4_ER_IRQn);

  /* USER CODE BEGIN I2C4_MspDeInit 1 */

  /* USER CODE END I2C4_MspDeInit 1 */
}

/* I2C3 init function */

__weak HAL_StatusTypeDef MX_I2C3_Init(I2C_HandleTypeDef* hi2c)
{
  HAL_StatusTypeDef ret = HAL_OK;

  hi2c->Instance = I2C3;
  hi2c->Init.Timing = 0x00F07BFF;
  hi2c->Init.OwnAddress1 = 0;
  hi2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c->Init.OwnAddress2 = 0;
  hi2c->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(hi2c) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  if (HAL_I2CEx_ConfigAnalogFilter(hi2c, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  if (HAL_I2CEx_ConfigDigitalFilter(hi2c, 0) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  return ret;
}

static void I2C3_MspInit(I2C_HandleTypeDef* i2cHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  /* USER CODE BEGIN I2C3_MspInit 0 */

  /* USER CODE END I2C3_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C3;
    PeriphClkInit.I2c3ClockSelection = RCC_I2C3CLKSOURCE_PCLK3;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    __HAL_RCC_GPIOG_CLK_ENABLE();
    /**I2C3 GPIO Configuration
    PG7     ------> I2C3_SCL
    PG8     ------> I2C3_SDA
    */
    GPIO_InitStruct.Pin = BUS_I2C3_SCL_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BUS_I2C3_SCL_GPIO_AF;
    HAL_GPIO_Init(BUS_I2C3_SCL_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BUS_I2C3_SDA_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BUS_I2C3_SDA_GPIO_AF;
    HAL_GPIO_Init(BUS_I2C3_SDA_GPIO_PORT, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_I2C3_CLK_ENABLE();
  /* USER CODE BEGIN I2C3_MspInit 1 */

  /* USER CODE END I2C3_MspInit 1 */
}

static void I2C3_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{
  /* USER CODE BEGIN I2C3_MspDeInit 0 */

  /* USER CODE END I2C3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C3_CLK_DISABLE();

    /**I2C3 GPIO Configuration
    PG7     ------> I2C3_SCL
    PG8     ------> I2C3_SDA
    */
    HAL_GPIO_DeInit(BUS_I2C3_SCL_GPIO_PORT, BUS_I2C3_SCL_GPIO_PIN);

    HAL_GPIO_DeInit(BUS_I2C3_SDA_GPIO_PORT, BUS_I2C3_SDA_GPIO_PIN);

  /* USER CODE BEGIN I2C3_MspDeInit 1 */

  /* USER CODE END I2C3_MspDeInit 1 */
}

/* I2C1 init function */

__weak HAL_StatusTypeDef MX_I2C1_Init(I2C_HandleTypeDef* hi2c)
{
  HAL_StatusTypeDef ret = HAL_OK;

  hi2c->Instance = I2C1;
  hi2c->Init.Timing = 0x00F07BFF;
  hi2c->Init.OwnAddress1 = 0;
  hi2c->Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c->Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c->Init.OwnAddress2 = 0;
  hi2c->Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c->Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c->Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(hi2c) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  if (HAL_I2CEx_ConfigAnalogFilter(hi2c, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  if (HAL_I2CEx_ConfigDigitalFilter(hi2c, 0) != HAL_OK)
  {
    ret = HAL_ERROR;
  }

  return ret;
}
DMA_HandleTypeDef handle_GPDMA1_Channel3;
DMA_HandleTypeDef handle_GPDMA1_Channel2;

static void I2C1_MspInit(I2C_HandleTypeDef* i2cHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  /* USER CODE BEGIN I2C1_MspInit 0 */

  /* USER CODE END I2C1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C1 GPIO Configuration
    PB6     ------> I2C1_SCL
    PB7     ------> I2C1_SDA
    */
    GPIO_InitStruct.Pin = BUS_I2C1_SCL_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BUS_I2C1_SCL_GPIO_AF;
    HAL_GPIO_Init(BUS_I2C1_SCL_GPIO_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BUS_I2C1_SDA_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BUS_I2C1_SDA_GPIO_AF;
    HAL_GPIO_Init(BUS_I2C1_SDA_GPIO_PORT, &GPIO_InitStruct);

    /* Peripheral clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();

    /* Peripheral DMA init*/

    handle_GPDMA1_Channel3.Instance = GPDMA1_Channel3;
    handle_GPDMA1_Channel3.Init.Request = GPDMA1_REQUEST_I2C1_TX;
    handle_GPDMA1_Channel3.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    handle_GPDMA1_Channel3.Init.Direction = DMA_MEMORY_TO_PERIPH;
    handle_GPDMA1_Channel3.Init.SrcInc = DMA_SINC_INCREMENTED;
    handle_GPDMA1_Channel3.Init.DestInc = DMA_DINC_FIXED;
    handle_GPDMA1_Channel3.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    handle_GPDMA1_Channel3.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    handle_GPDMA1_Channel3.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
    handle_GPDMA1_Channel3.Init.SrcBurstLength = 1;
    handle_GPDMA1_Channel3.Init.DestBurstLength = 1;
    handle_GPDMA1_Channel3.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
    handle_GPDMA1_Channel3.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    handle_GPDMA1_Channel3.Init.Mode = DMA_NORMAL;
    HAL_DMA_Init(&handle_GPDMA1_Channel3);

    __HAL_LINKDMA(i2cHandle, hdmatx, handle_GPDMA1_Channel3);

    HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel3, DMA_CHANNEL_NPRIV);

    handle_GPDMA1_Channel2.Instance = GPDMA1_Channel2;
    handle_GPDMA1_Channel2.Init.Request = GPDMA1_REQUEST_I2C1_RX;
    handle_GPDMA1_Channel2.Init.BlkHWRequest = DMA_BREQ_SINGLE_BURST;
    handle_GPDMA1_Channel2.Init.Direction = DMA_PERIPH_TO_MEMORY;
    handle_GPDMA1_Channel2.Init.SrcInc = DMA_SINC_FIXED;
    handle_GPDMA1_Channel2.Init.DestInc = DMA_DINC_INCREMENTED;
    handle_GPDMA1_Channel2.Init.SrcDataWidth = DMA_SRC_DATAWIDTH_BYTE;
    handle_GPDMA1_Channel2.Init.DestDataWidth = DMA_DEST_DATAWIDTH_BYTE;
    handle_GPDMA1_Channel2.Init.Priority = DMA_LOW_PRIORITY_LOW_WEIGHT;
    handle_GPDMA1_Channel2.Init.SrcBurstLength = 1;
    handle_GPDMA1_Channel2.Init.DestBurstLength = 1;
    handle_GPDMA1_Channel2.Init.TransferAllocatedPort = DMA_SRC_ALLOCATED_PORT0|DMA_DEST_ALLOCATED_PORT0;
    handle_GPDMA1_Channel2.Init.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
    handle_GPDMA1_Channel2.Init.Mode = DMA_NORMAL;
    HAL_DMA_Init(&handle_GPDMA1_Channel2);

    __HAL_LINKDMA(i2cHandle, hdmarx, handle_GPDMA1_Channel2);

    HAL_DMA_ConfigChannelAttributes(&handle_GPDMA1_Channel2, DMA_CHANNEL_NPRIV);

    /* Peripheral interrupt init */
    HAL_NVIC_SetPriority(I2C1_EV_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_SetPriority(I2C1_ER_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
  /* USER CODE BEGIN I2C1_MspInit 1 */

  /* USER CODE END I2C1_MspInit 1 */
}

static void I2C1_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{
  /* USER CODE BEGIN I2C1_MspDeInit 0 */

  /* USER CODE END I2C1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C1_CLK_DISABLE();

    /**I2C1 GPIO Configuration
    PB6     ------> I2C1_SCL
    PB7     ------> I2C1_SDA
    */
    HAL_GPIO_DeInit(BUS_I2C1_SCL_GPIO_PORT, BUS_I2C1_SCL_GPIO_PIN);

    HAL_GPIO_DeInit(BUS_I2C1_SDA_GPIO_PORT, BUS_I2C1_SDA_GPIO_PIN);

    /* Peripheral DMA DeInit*/
    HAL_DMA_DeInit(i2cHandle->hdmatx);
    HAL_DMA_DeInit(i2cHandle->hdmarx);

    /* Peripheral interrupt Deinit*/
    HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);

    HAL_NVIC_DisableIRQ(I2C1_ER_IRQn);

  /* USER CODE BEGIN I2C1_MspDeInit 1 */

  /* USER CODE END I2C1_MspDeInit 1 */
}

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

