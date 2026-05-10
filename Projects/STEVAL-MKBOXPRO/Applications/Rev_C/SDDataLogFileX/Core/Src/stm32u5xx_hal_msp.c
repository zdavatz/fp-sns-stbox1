/**
  ******************************************************************************
  * @file    stm32u5xx_hal_msp.c
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    20-May-2025
  * @brief   This file provides code for the MSP Initialization
  *          and de-Initialization codes.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* External functions --------------------------------------------------------*/

/**
  * Initializes the Global MSP.
  */
void HAL_MspInit(void)
{
  /* USER CODE BEGIN MspInit 0 */

  /* USER CODE END MspInit 0 */

  __HAL_RCC_PWR_CLK_ENABLE();

  /* System interrupt init*/

  /** Disable the internal Pull-Up in Dead Battery pins of UCPD peripheral
  */
  HAL_PWREx_DisableUCPDDeadBattery();

  /* USER CODE BEGIN MspInit 1 */
  /* Match BLEDualProgram's SystemPower_Config exactly: VddIO2 enable +
     SMPS regulator. The SMPS path is what powers the chip's RF rail
     reliably during transmit; without it, BLE init succeeds (HCI, GATT,
     advertising commands all return SUCCESS) but the BlueNRG-LP never
     actually radiates — exactly the "advertising started but invisible
     in scanners" symptom. */
  HAL_PWREx_EnableVddIO2();
  if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK) {
    /* Non-fatal: fall back to LDO. The logger keeps working; only BLE
       transmit needs SMPS reliably. */
  }
  /* USER CODE END MspInit 1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
