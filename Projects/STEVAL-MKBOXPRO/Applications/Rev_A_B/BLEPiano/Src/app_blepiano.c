/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_blepiano.c
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
  * @page BLEPiano Secure BLE Firmware Over the Air Update
  *
  * @image html st_logo.png
  *
  * <b>Introduction</b>
  *
  * This firmware package includes Components Device Drivers, Board Support Package
  * and example application for the following STMicroelectronics elements:
  * - STEVAL-MKBOXPRO (SensorTile.box-Pro) evaluation board that contains the following components:
  *   - MEMS sensor devices: STTS22, LPS22DF, LSM6DSV16X, LIS2DU12, LIS2MDL
  *   - Gas Gouge device: STC3115
  *   - Digital Microphone: MP23db01HP
  *   - Dynamic NFC tag: ST25DV04K
  *   - BlueNRG-LP Bluetooth Low Energy System On Chip
  *
  * <b>Example Application</b>
  *
  * This example explains how using bluetooth it is possible to play Music Notes on .box-Pro
  * This example is compatible with the Firmware Over the Air Update (FoTA)
  *
  * This example must be used with the related ST BLE Sensor Android/iOS application available on Play/itune store
  * (Version 5.2.0 or higher), in order to read the sent information by Bluetooth Low Energy protocol
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "app_blepiano.h"
#include "bluenrg_conf.h"
#include "ble_function.h"
#include "ota.h"
#include "stbox1_config.h"
#include "note.h"
#include "SensorTileBoxPro_gg.h"
#include "steval_mkboxpro.h"

/* Exported variables --------------------------------------------------------*/
uint16_t ConnectionHandle = 0;
/* Memory Bank used */
int32_t CurrentActiveBank = 0;

FinishGood_TypeDef FinishGood;

void *HandleGGComponent;

/* Identify if battery is present or not */
uint8_t BatteryPresent = 0;

/* Private define ------------------------------------------------------------*/
#define SPKR_PIN GPIO_PIN_13
#define SPKR_GPIO_PORT GPIOE

#define TIMERS_PERIOD 65535

/* Private macro -------------------------------------------------------------*/
#define MCR_START_HEART_BIT()   \
  {                         \
    BSP_LED_On(LED_YELLOW); \
    BSP_LED_On(LED_GREEN);  \
    beep(NOTE_A6, 50);      \
    HAL_Delay(150);         \
    BSP_LED_Off(LED_YELLOW);\
    BSP_LED_Off(LED_GREEN); \
    HAL_Delay(400);         \
    BSP_LED_On(LED_YELLOW); \
    BSP_LED_On(LED_GREEN);  \
    beep(NOTE_G6, 50);      \
    HAL_Delay(150);         \
    BSP_LED_Off(LED_YELLOW);\
    BSP_LED_Off(LED_GREEN); \
    HAL_Delay(400);         \
    beep(NOTE_F6, 50);      \
  }

#define MCR_START_HEART_BIT2()  \
  {                         \
    BSP_LED_On(LED_YELLOW); \
    BSP_LED_On(LED_RED);    \
    beep(NOTE_F6, 50);      \
    HAL_Delay(150);         \
    BSP_LED_Off(LED_YELLOW);\
    BSP_LED_Off(LED_RED);   \
    HAL_Delay(400);         \
    BSP_LED_On(LED_YELLOW); \
    BSP_LED_On(LED_RED);    \
    beep(NOTE_G6, 50);      \
    HAL_Delay(150);         \
    BSP_LED_Off(LED_YELLOW);\
    BSP_LED_Off(LED_RED);   \
    HAL_Delay(400);         \
    beep(NOTE_A6, 50);      \
  }

/* Private variables ---------------------------------------------------------*/
static volatile uint32_t user_button_pressed = 0;
/* Enable led blinking */
static volatile uint32_t BlinkLed = 0;

/* Enable to send battery info data via Bluetooth */
static volatile uint32_t SendBatteryInfo = 0;

/* Private Functions prototypes ----------------------------------------------*/
static void User_Init(void);
static void PrintInfo(void);

static FinishGood_TypeDef BSP_CheckFinishGood(void);

static void SendBatteryInfoData(void);
static void TIM_Beep_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported Functions --------------------------------------------------------*/
/**
  * @brief  The application entry point.
  * @retval none
  */
void MX_BLEPiano_Init(void)
{
  /* Set a random seed */
  srand(HAL_GetTick());

  User_Init();

  STBOX1_PRINTF("\033[2J"); /* serial console clear screen */
  STBOX1_PRINTF("\033[H");  /* serial console cursor to home */
  PrintInfo();

  BSP_LED_On(LED_BLUE);

  /* Init Gas Gouge */
  {
    uint32_t flag;
    DrvStatusTypeDef InitType = BSP_GG_Init(&HandleGGComponent);

    switch (InitType)
    {
      case COMPONENT_OK:
        STBOX1_PRINTF("Gas Gouge Component OK\n\r");
        BSP_GG_GetPresence(HandleGGComponent, &flag);
        if (flag)
        {
          STBOX1_PRINTF("\tBattery present\n\r");
          BatteryPresent = 1;
        }
        else
        {
          STBOX1_PRINTF("\tBattery not present\n\r");
          BatteryPresent = 0;
        }
        break;
      case COMPONENT_BATT_FAIL:
        STBOX1_PRINTF("Gas Gouge Component OK\n\r");
        STBOX1_PRINTF("\tBattery not present\n\r");
        BatteryPresent = 0;
        break;
      default:
        STBOX1_PRINTF("Gas Gouge Component ERROR\n\r");
        BatteryPresent = 0;
    }
  }

  /* Init BLE */
  STBOX1_PRINTF("\r\nInitializing Bluetooth\r\n");
  bluetooth_init();

  /* FOTA and Dual Banks Section */
  {
    uint16_t FwId1;
    uint16_t FwId2;

    /* Now update the BLE advertize data and make the Board connectable */
    enable_extended_configuration_command();
    ble_extended_configuration_value.banks_swap = 0;

    ReadFlashBanksFwId(&FwId1, &FwId2);
    if (FwId2 != OTA_OTA_FW_ID_NOT_VALID)
    {
      /* Enable the Banks Swap only if there is a valid fw on second bank */
      ble_extended_configuration_value.banks_swap = 1;
    }
  }

  BSP_LED_Off(LED_BLUE);
  BSP_LED_On(LED_GREEN);

  /* Short delay before starting the user application process */
  HAL_Delay(500);
  STBOX1_PRINTF("BLE Stack Initialized & Device Configured\r\n");

}

/*
 * FP-SNS-STBOX1 background task
 */
void MX_BLEPiano_Process(void)
{
  if (hci_event)
  {
    hci_event = 0;
    hci_user_evt_proc();
  }

  /* Make the device discoverable */
  if (set_connectable)
  {
    uint32_t uhCapture = __HAL_TIM_GET_COUNTER(&TIM_CC_HANDLE);
    /* Start the TIM Base generation in interrupt mode */
    if (HAL_TIM_OC_Start_IT(&TIM_CC_HANDLE, TIM_CHANNEL_1) != HAL_OK)
    {
      /* Starting Error */
      STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
    }
    /* Set the Capture Compare Register value */
    __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_1, (uhCapture + STBOX1_UPDATE_LED));

    set_connectable_ble();
    set_connectable = FALSE;

  }

  /* Send Battery Info */
  if (SendBatteryInfo)
  {
    SendBatteryInfo = 0;
    SendBatteryInfoData();
  }

  /* Reboot the Board */
  if (RebootBoard)
  {
    RebootBoard = 0;
    HAL_NVIC_SystemReset();
  }

  /* Swap the Flash Banks */
  if (SwapBanks)
  {
    EnableDisableDualBoot();
    SwapBanks = 0;
  }

  /* Handle the user button */
  if (user_button_pressed)
  {
    user_button_pressed = 0;
    STBOX1_PRINTF("User Button pressed...\r\n");
  }

  /* Blinking the Led */
  if (BlinkLed)
  {
    BlinkLed = 0;
    BSP_LED_Toggle(LED_GREEN);
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
  BSP_LED_Off(LED_RED);
  STBOX1_PRINTF("Error at %ld at %s\r\n", Line, File);
  while (1)
  {
    int32_t count;
    for (count = 0; count < ErrorCode; count++)
    {
      BSP_LED_On(LED_RED);
      HAL_Delay(500);
      BSP_LED_Off(LED_RED);
      HAL_Delay(2000);
    }
    BSP_LED_On(LED_GREEN);
    BSP_LED_On(LED_YELLOW);
    HAL_Delay(2000);
    BSP_LED_Off(LED_GREEN);
    BSP_LED_Off(LED_YELLOW);
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
  BSP_LED_Init(LED_RED);
  BSP_LED_Init(LED_YELLOW);
  BSP_LED_Init(LED_BLUE);

  /* why RED is activated by default? */
  BSP_LED_Off(LED_RED);

  TIM_Beep_Init();

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
      MCR_START_HEART_BIT2();
    }
    else
    {
      CurrentActiveBank = 1;
      MCR_START_HEART_BIT();
    }
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
  }

  BSP_COM_Init(COM1);

  TIM_Beep_DeInit();
  InitTimers();

  /* Check the board Type */
  FinishGood = BSP_CheckFinishGood();

  /* Update the Current Fw ID saved in flash if it's neceessary */
  if (FinishGood == FINISHA)
  {
    UpdateCurrFlashBankFwIdBoardName(STBOX1A_BLUEST_SDK_FW_ID, NULL);
  }
  else
  {
    UpdateCurrFlashBankFwIdBoardName(STBOX1B_BLUEST_SDK_FW_ID, NULL);
  }
}

/**
  * @brief  Print Bunner
  * @param  None
  * @retval None
  */
static void PrintInfo(void)
{
  if (FinishGood == FINISH_ERROR)
  {
    STBOX1_Error_Handler(STBOX1_ERROR_HW_INIT, __FILE__, __LINE__);
  }

  STBOX1_PRINTF("\r\nSTMicroelectronics %s:\r\n"
                "\tVersion %c.%c.%c\r\n"
                "\tSTEVAL-MKBOXPRO Rev %c board"
                "\r\n",
                STBOX1_PACKAGENAME,
                STBOX1_VERSION_MAJOR, STBOX1_VERSION_MINOR, STBOX1_VERSION_PATCH,
                (FinishGood == FINISHA) ? 'A' : 'B');
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

/**
  * @brief  GPIO Configuration for Beep timer
  * @param  TIM_HandleTypeDef *htim Timer handle
  * @retval None
  */
static void TIM_Beep_MspPostInit(TIM_HandleTypeDef *htim)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if (htim->Instance == TIM_CC_INSTANCE)
  {
    /* USER CODE BEGIN TIM1_MspPostInit 0 */
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* USER CODE END TIM1_MspPostInit 0 */
    BLEPIANO_TIMx_CLK_ENABLE();
    /**TIM1 GPIO Configuration
    PE13     ------> TIM1_CH3
      */
    GPIO_InitStruct.Pin = SPKR_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BLEPIANO_GPIO_AF1_TIMx;
    HAL_GPIO_Init(SPKR_GPIO_PORT, &GPIO_InitStruct);

    /* USER CODE BEGIN TIM1_MspPostInit 1 */

    /* USER CODE END TIM1_MspPostInit 1 */
  }
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
void TIM_Beep_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */
  memset(&TIM_CC_HANDLE, 0, sizeof(TIM_HandleTypeDef));

  /* USER CODE END TIM1_Init 1 */
  TIM_CC_HANDLE.Instance = TIM_CC_INSTANCE;
  TIM_CC_HANDLE.Init.Prescaler = 1000;
  TIM_CC_HANDLE.Init.CounterMode = TIM_COUNTERMODE_UP;
  TIM_CC_HANDLE.Init.Period = TIMERS_PERIOD;
  TIM_CC_HANDLE.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  TIM_CC_HANDLE.Init.RepetitionCounter = 0;
  TIM_CC_HANDLE.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&TIM_CC_HANDLE) != HAL_OK)
  {
    while (1);
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&TIM_CC_HANDLE, &sClockSourceConfig) != HAL_OK)
  {
    while (1);
  }
  if (HAL_TIM_PWM_Init(&TIM_CC_HANDLE) != HAL_OK)
  {
    while (1);
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&TIM_CC_HANDLE, &sMasterConfig) != HAL_OK)
  {
    while (1);
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&TIM_CC_HANDLE, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    while (1);
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&TIM_CC_HANDLE, &sBreakDeadTimeConfig) != HAL_OK)
  {
    while (1);
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  TIM_Beep_MspPostInit(&TIM_CC_HANDLE);
}

/**
  * @brief TIM1 DeInitialization Function
  * @param None
  * @retval None
  */
void TIM_Beep_DeInit(void)
{
  /* Reset the Timer Used for making the Beep*/
  HAL_GPIO_DeInit(SPKR_GPIO_PORT, SPKR_PIN);
  BLEPIANO_TIMx_FORCE_RESET();
  HAL_Delay(10);
  BLEPIANO_TIMx_RELEASE_RESET();
  HAL_Delay(10);
}

/**
  * @brief  Send Battery Info Data (Voltage/Current/Soc) to BLE
  * @param  uint32_t freq
  * @param  uint16_t time
  * @retval None
  */
void beep(uint32_t freq, uint16_t time)
{
  TIM_OC_InitTypeDef sConfigOC = {0};
  uint16_t period = (SystemCoreClock / 100) / freq - 1;
  uint16_t DutyCycle = period / 2;

  if (freq == 0)
  {
    HAL_TIM_PWM_Stop(&TIM_CC_HANDLE, TIM_CHANNEL_3);
    return; /* speaker off */
  }

  TIM_CC_HANDLE.Instance = TIM_CC_INSTANCE;
  TIM_CC_HANDLE.Init.Prescaler = 63; /* because 64MHz of timer clock, to have 1MHz of clock freq */
  TIM_CC_HANDLE.Init.CounterMode = TIM_COUNTERMODE_UP;
  TIM_CC_HANDLE.Init.Period = period;
  TIM_CC_HANDLE.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  TIM_CC_HANDLE.Init.RepetitionCounter = 0;
  TIM_CC_HANDLE.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&TIM_CC_HANDLE) != HAL_OK)
  {
    while (1);
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = DutyCycle;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&TIM_CC_HANDLE, &sConfigOC, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&TIM_CC_HANDLE, TIM_CHANNEL_3);
  if (time) /* don't stop beep if time=0 */
  {
    HAL_Delay(time);
    HAL_TIM_PWM_Stop(&TIM_CC_HANDLE, TIM_CHANNEL_3);
  }
}

/**
  * @brief  Initialize Timers
  * @param  None
  * @retval None
  */
void InitTimers(void)
{
  uint32_t uwPrescalerValue;

  /* Timer Output Compare Configuration Structure declaration */
  TIM_OC_InitTypeDef sConfig;

  /* Compute the prescaler value to counter clock equal to 10000 Hz */
  uwPrescalerValue = (uint32_t)((SystemCoreClock / 10000) - 1);

  memset(&TIM_CC_HANDLE, 0, sizeof(TIM_HandleTypeDef));

  /* Set TIM instance */
  TIM_CC_HANDLE.Instance           = TIM_CC_INSTANCE;
  TIM_CC_HANDLE.Init.Period        = TIMERS_PERIOD;
  TIM_CC_HANDLE.Init.Prescaler     = uwPrescalerValue;
  TIM_CC_HANDLE.Init.ClockDivision = 0;
  TIM_CC_HANDLE.Init.CounterMode   = TIM_COUNTERMODE_UP;

  if (HAL_TIM_OC_DeInit(&TIM_CC_HANDLE) != HAL_OK)
  {
    /* Initialization Error */
    STBOX1_Error_Handler(STBOX1_ERROR_HW_INIT, __FILE__, __LINE__);
  }

  if (HAL_TIM_OC_Init(&TIM_CC_HANDLE) != HAL_OK)
  {
    /* Initialization Error */
    STBOX1_Error_Handler(STBOX1_ERROR_HW_INIT, __FILE__, __LINE__);
  }

  /* Configure the Output Compare channels */

  /* Common configuration for all channels */
  sConfig.OCMode     = TIM_OCMODE_TOGGLE;
  sConfig.OCPolarity = TIM_OCPOLARITY_LOW;

  sConfig.Pulse = STBOX1_UPDATE_LED;
  if (HAL_TIM_OC_ConfigChannel(&TIM_CC_HANDLE, &sConfig, TIM_CHANNEL_1) != HAL_OK)
  {
    /* Configuration Error */
    STBOX1_Error_Handler(STBOX1_ERROR_HW_INIT, __FILE__, __LINE__);
  }
}

/**
  * @brief  Send Battery Info Data (Voltage/Current/Soc) to BLE
  * @param  None
  * @retval None
  */
static void SendBatteryInfoData(void)
{
  uint32_t BatteryLevel;
  int32_t Current = 0;

  uint32_t Voltage;
  uint8_t v_mode;
  uint32_t Status  = 0x04; /* Unknown */

  if (BatteryPresent)
  {
    /* Update Gas Gouge Status */
    BSP_GG_Task(HandleGGComponent, &v_mode);

    /* Read the Gas Gouge Status */
    BSP_GG_GetVoltage(HandleGGComponent, &Voltage);
    BSP_GG_GetCurrent(HandleGGComponent, &Current);
    BSP_GG_GetSOC(HandleGGComponent, &BatteryLevel);

    /* if it's < 15% Low Battery */
    if (BatteryLevel < 15)
    {
      Status = 0x00; /* Low Battery */
    }
    else
    {
      if (Current < 0)
      {
        Status = 0x01; /* Discharging */
      }
      else
      {
        Status = 0x03; /* Charging */
      }
    }
  }
  else
  {
    Status = 0x02; /* Plugged Not Charging */
    Current = 0x8000;
    Voltage = 0;
    BatteryLevel = 0;
  }

  ble_battery_update(BatteryLevel, Voltage, Current / 10, Status);
}

/**
  * @brief  This method the Finish Good type
  * @retval FinishGood value
  */
static FinishGood_TypeDef BSP_CheckFinishGood(void)
{
#define ST25_ADDR_DATA_I2C                ((uint8_t)0xAE)
#define ST25_ICREF_REG                    ((uint16_t)0x0017)
  /* ST25DVxxKC 4Kbits ICref */
#define IAM_ST25DV04KC                    0x50U
  /* ST25DVxxKC 16/64Kbits ICref */
#define IAM_ST25DV64KC                    0x51U
  /* @brief ST25DV 4Kbits ICref */
#define IAM_ST25DV04                      0x24
  /* @brief ST25DV 16/64Kbits ICref */
#define IAM_ST25DV64                      0x26

  FinishGood_TypeDef FinishGood = FINISH_ERROR;
  uint8_t nfctag_id;
  BSP_ST25DV_I2C_INIT();

  BSP_ST25DV_I2C_READ_REG_16(ST25_ADDR_DATA_I2C, ST25_ICREF_REG, &nfctag_id, 1);

  if ((nfctag_id == IAM_ST25DV04KC) | (nfctag_id == IAM_ST25DV64KC))
  {
    FinishGood = FINISHB;
  }
  else if ((nfctag_id == IAM_ST25DV04) | (nfctag_id == IAM_ST25DV64))
  {
    FinishGood = FINISHA;
  }

  BSP_ST25DV_I2C_DEINIT();

#undef ST25_ADDR_DATA_I2C
#undef ST25_ICREF_REG
#undef IAM_ST25DV04KC
#undef IAM_ST25DV64KC
#undef IAM_ST25DV04
#undef IAM_ST25DV64

  return FinishGood;
}

/* Callback Functions --------------------------------------------------------*/
/**
  * @brief  BSP Push Button callback
  * @param  Button Specifies the pin connected EXTI line
  * @retval None
  */
void BSP_PB_Callback(Button_TypeDef Button)
{
  /* Set the User Button flag */
  user_button_pressed = 1;
}

/**
  * @brief  Output Compare callback in non blocking mode
  * @param  TIM_HandleTypeDef *htim TIM OC handle
  * @retval None
  */
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
  uint32_t uhCapture = 0;
  /* TIM1_CH1 toggling with frequency = 1Hz */
  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
  {
    uhCapture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
    /* Set the Capture Compare Register value */
    __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_1, (uhCapture + STBOX1_UPDATE_LED));
    if (W2ST_CHECK_CONNECTION(W2ST_CONNECT_BAT_EVENT))
    {
      SendBatteryInfo = 1;
    }
    else
    {
      BlinkLed = 1;
    }
  }

}

