/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_nfc_ftm.c
  * @author  System Research & Applications Team - Agrate/Catania Lab.
  * @brief   Main program body
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

/**
  *
  * @page NFC_FTM NFC Fast Transfer Memory protocol
  *
  * @image html st_logo.png
  *
  * <b>Introduction</b>
  *
  * This firmware package includes Components Device Drivers, Board Support Package
  * and example application for the following STMicroelectronics elements:
  * - STEVAL-STWINBX1 (STWIN.box) evaluation board that contains the following components:
  *   - MEMS sensor devices: IIS2DLPC, IIS2MDC, IIS3DWB, ISM330DHCX, IIS2ICLX, ILPS22QS, STTS22H
  *   - analog (IMP23ABSU) and digital (IMP34DT05) microphones
  *   - dynamic NFC tag: ST25DV64
  *   - BlueNRG-2 Bluetooth Low Energy System On Chip
  *
  * <b>Example Application</b>
  *
  * The Example application provides one example on how to use the Fast Memory Transfer protocol
  * for making a firmware update using the NFC and using
  * the STMicroelectronics ST25 NFC Tap available for Android and iOS:
  *   - https://play.google.com/store/apps/details?id=com.st.st25nfc
  *   - https://apps.apple.com/be/app/nfc-tap/id1278913597
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "app_nfc_ftm.h"
#include "stbox1_config.h"
#include "STWIN.box_nfctag.h"
#include "st25ftm_config.h"
#include "st25ftm_process.h"
#include "steval_stwinbx1.h"

/* Exported variables --------------------------------------------------------*/
/* Memory Bank used */
int32_t CurrentActiveBank = 0;

#define TIMERS_PERIOD 65535

/* Private variables ---------------------------------------------------------*/
static volatile uint32_t user_button_pressed = 0;

/* Private Functions prototypes ----------------------------------------------*/
static void User_Init(void);
static void PrintInfo(void);

/* Exported Functions --------------------------------------------------------*/
/**
  * @brief  The application entry point.
  * @retval none
  */
void MX_NFC_FTM_Init(void)
{
  User_Init();

  STBOX1_PRINTF("\033[2J"); /* serial console clear screen */
  STBOX1_PRINTF("\033[H");  /* serial console cursor to home */
  PrintInfo();

  /* Init ST25DV driver */
  while (BSP_NFCTAG_Init(BSP_NFCTAG_INSTANCE) != NFCTAG_OK);

  /* Setting the New Password for I2C protection */
  if (BSP_NFCTAG_ChangeI2CPassword(STBOX1_MSB_PASSWORD, STBOX1_LSB_PASSWORD) != NFCTAG_OK)
  {
    STBOX1_PRINTF("Error NFC Changing the I2C password\r\n");
    STBOX1_Error_Handler(STBOX1_ERROR_NFC, __FILE__, __LINE__);
  }
  else
  {
    STBOX1_PRINTF("NFC Changed the I2C password\r\n");
  }

  /* Enable the NFC GPIO Interrupt */
  if (BSP_NFCTAG_GPIO_Init() != BSP_ERROR_NONE)
  {
    STBOX1_PRINTF("Error NFC Initializing GPIO\r\n");
    STBOX1_Error_Handler(STBOX1_ERROR_NFC, __FILE__, __LINE__);
  }
  else
  {
    STBOX1_PRINTF("NFC NFC GPIO Initialized\r\n");
  }

  STBOX1_PRINTF("Press User Button for Enabling/Disabling FTM\r\n");
}

/*
 * FP-SNS-STBOX1 background task
 */
void MX_NFC_FTM_Process(void)
{
  static uint32_t MailBoxEnabled = 0;

  /* FTM Work Function*/
  if (MailBoxEnabled == 1)
  {
    FTMManagement();
  }

  /* Handle the user button */
  if (user_button_pressed)
  {
    /* Reset Interrupt flag */
    user_button_pressed = 0;

    if (MailBoxEnabled)
    {
      MailBoxEnabled = 0;
      FTMManagementDeInit();
    }
    else
    {
      MailBoxEnabled = 1;
      FTMManagementInit();
    }
  }

  /* Wait next event */
  __WFI();
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param int32_t ErrorCode Error Code
  * @retval None
  */
void STBOX1_Error_Handler(int32_t ErrorCode, char *File, int32_t Line)
{
  /* User can add his own implementation to report the HAL error return state */
  BSP_LED_Off(LED_ORANGE);
  STBOX1_PRINTF("Error at %ld at %s\r\n", Line, File);
  while (1)
  {
    int32_t count;
    for (count = 0; count < ErrorCode; count++)
    {
      BSP_LED_On(LED_ORANGE);
      HAL_Delay(500);
      BSP_LED_Off(LED_ORANGE);
      HAL_Delay(2000);
    }
    BSP_LED_On(LED_GREEN);
    HAL_Delay(2000);
    BSP_LED_Off(LED_GREEN);
  }
}

/* Private Functions ---------------------------------------------------------*/
/**
  * @brief  Initialize User process
  * @param  None
  * @retval None
  */
static void User_Init(void)
{
  /* Enable Button in Interrupt mode */
  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);

  /* Init the Led */
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_ORANGE);

  /* Check if we are running from Bank1 or Bank2 */
  {
    FLASH_OBProgramInitTypeDef    OBInit;
    /* Allow Access to Flash control registers and user Flash */
    HAL_FLASH_Unlock();
    /* Allow Access to option bytes sector */
    HAL_FLASH_OB_Unlock();
    /* Get the Dual boot configuration status */
    HAL_FLASHEx_OBGetConfig(&OBInit);
    if (((OBInit.USERConfig) & (OB_SWAP_BANK_ENABLE)) == OB_SWAP_BANK_ENABLE)
    {
      CurrentActiveBank = 2;
      MCR_HEART_BIT2();
    }
    else
    {
      CurrentActiveBank = 1;
      MCR_HEART_BIT();
    }
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
  }

  BSP_COM_Init(COM1);

}

/**
  * @brief  Print Bunner
  * @param  None
  * @retval None
  */
static void PrintInfo(void)
{
  STBOX1_PRINTF("\r\nSTMicroelectronics %s:\r\n"
                "\tVersion %d.%d.%d\r\n"
                "\tSTEVAL-STWINBX1 board"
                "\r\n",
                STBOX1_PACKAGENAME,
                STBOX1_VERSION_MAJOR, STBOX1_VERSION_MINOR, STBOX1_VERSION_PATCH);
  STBOX1_PRINTF("\t(HAL %ld.%ld.%ld_%ld)\r\n"
                "\tCompiled %s %s"
#if defined (__IAR_SYSTEMS_ICC__)
                " (IAR)\r\n",
#elif defined (__ARMCC_VERSION)
                " (KEIL)\r\n",
#elif defined (__GNUC__)
                " (STM32CubeIDE)\r\n",
#endif /* IDE */
                HAL_GetHalVersion() >> 24,
                (HAL_GetHalVersion() >> 16) & 0xFF,
                (HAL_GetHalVersion() >> 8) & 0xFF,
                HAL_GetHalVersion()      & 0xFF,
                __DATE__, __TIME__);
  STBOX1_PRINTF("Current Bank =%ld\r\n", CurrentActiveBank);
}

/* Callback Functions --------------------------------------------------------*/
/**
  * @brief  BSP Push Button callback
  * @param  Button Specifies the pin connected EXTI line
  * @retval None
  */
void BSP_PB_Callback(Button_TypeDef Button)
{
  if (Button == BUTTON_USER)
  {
    /* Set the User Button flag */
    user_button_pressed = SET;
  }
}

/**
  * @brief  BSP NFCTAG GPIO callback
  * @param  Node
  * @retval None.
  */
void BSP_NFCTAG_GPIO_Callback(void)
{
  /* Set the GPIO flag */
  GPO_Activated = 1;
}

#if defined (__ARMCC_VERSION)
extern UART_HandleTypeDef hcom_uart[];

/** @brief fgetc call for standard input implementation
  * @param f File pointer
  * @retval Character acquired from standard input
  */
int fgetc(FILE *f)
{
  int32_t ch;
  (void)HAL_UART_Receive(&hcom_uart[COM1], (uint8_t *) &ch, 1, COM_POLL_TIMEOUT);
  return ch;
}
#endif /* defined (__ARMCC_VERSION) */

