/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_bledualprogram.c
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
  * @page BLEDualProgram Secure BLE Firmware Over the Air Update
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
  * The Example application initializes all the Components and Library creating some Custom Bluetooth services
  * At the Beginning the application writes on NFC tag the URL of st.com, after pressing the user button,
  * the application changes the content of NFC adding a deep URI for automatic connection to the board
  *
  * The application allows connections only from bonded devices (PIN request) allowing them to updated its firmware
  * with one Over the Air update (FoTA)
  *
  * This example must be used with the related ST BLE Sensor Android/iOS application available on Play/itune store
  * (Version 5.2.0 or higher), in order to read the sent information by Bluetooth Low Energy protocol
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "app_bledualprogram.h"
#include "stm32wb07_06_conf.h"
#include "ble_function.h"
#include "ota.h"
#include "stbox1_config.h"

#ifdef STBOX1_ENABLE_START_BIP
#include "note.h"
#endif /* STBOX1_ENABLE_START_BIP */

#include "tagtype5_wrapper.h"
#include "lib_NDEF_URI.h"
#include "SensorTileBoxPro_nfctag.h"
#include "steval_mkboxpro.h"

/* Exported variables --------------------------------------------------------*/
uint16_t ConnectionHandle = 0;
/* Memory Bank used */
int32_t CurrentActiveBank = 0;

/* Private define ------------------------------------------------------------*/
#ifdef STBOX1_ENABLE_START_BIP
#define SPKR_PIN GPIO_PIN_13
#define SPKR_GPIO_PORT GPIOE
#endif /* STBOX1_ENABLE_START_BIP*/

#define TIMERS_PERIOD 65535

/* Private macro -------------------------------------------------------------*/
#ifdef STBOX1_ENABLE_START_BIP
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
#else /* STBOX1_ENABLE_START_BIP */
#define MCR_START_HEART_BIT MCR_HEART_BIT
#define MCR_START_HEART_BIT2 MCR_HEART_BIT2
#endif /* STBOX1_ENABLE_START_BIP */

/* Private variables ---------------------------------------------------------*/
static volatile uint32_t user_button_pressed = 0;
/* Enable led blinking */
static volatile uint32_t BlinkLed = 0;

/* Enable to send inertial data via Bluetooth */
static volatile uint32_t SendAccGyroMag = 0;

/* Enable to send environmental data via Bluetooth */
static volatile uint32_t SendEnv = 0;

/* Enable to send quaternion data via Bluetooth */
static volatile uint32_t SendQuat = 0;

/* Private Functions prototypes ----------------------------------------------*/
static void User_Init(void);
static void PrintInfo(void);

#ifdef STBOX1_ENABLE_START_BIP
static void TIM_Beep_MspPostInit(TIM_HandleTypeDef *htim);
static void TIM_Beep_Init(void);
static void TIM_Beep_DeInit(void);
static void beep(uint32_t freq, uint16_t time);
#endif /* STBOX1_ENABLE_START_BIP */

static void InitTimers(void);

void NDEF_URI_Init(void);

static void Set_Random_Motion_Values(ble_manager_inertial_axes_t *x_axes,
                                     ble_manager_inertial_axes_t *g_axes,
                                     ble_manager_inertial_axes_t *m_axes,
                                     ble_motion_sensor_axes_t *q_axes,
                                     uint32_t cnt);
static void Reset_Motion_Values(ble_manager_inertial_axes_t *x_axes,
                                ble_manager_inertial_axes_t *g_axes,
                                ble_manager_inertial_axes_t *m_axes,
                                ble_motion_sensor_axes_t *q_axes);

static void ClearSecureDB(void);
static void BLE_Pairing_ST25DV(void);

/* Exported Functions --------------------------------------------------------*/
/**
  * @brief  The application entry point.
  * @retval none
  */
void MX_BLEDualProgram_Init(void)
{
  /* Set a random seed */
  srand(HAL_GetTick());

  User_Init();

  STBOX1_PRINTF("\033[2J"); /* serial console clear screen */
  STBOX1_PRINTF("\033[H");  /* serial console cursor to home */
  PrintInfo();

  BSP_LED_On(LED_BLUE);

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

  /* This is for Writing a URI..."st.com" */
  NDEF_URI_Init();
}

/*
 * FP-SNS-STBOX1 background task
 */
void MX_BLEDualProgram_Process(void)
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

    /* If we need to clear the Secure DataBase */
    if (NeedToClearSecureDB)
    {
      NeedToClearSecureDB = 0;
      ClearSecureDB();
    }
  }

  /*  Update sensor value */
  if (SendEnv)
  {
    float data_t;
    float data_p;
    SendEnv = 0;
    /* Update emulated Environmental data */
    Set_Random_Environmental_Values(&data_t, &data_p);

    if (paired == TRUE)
    {
      /* Send Values only if we are paired with the Device */
      ble_environmental_update((int32_t)(data_p * 100), 0, (int16_t)(data_t * 10), 0);
    }
  }

  if ((SendAccGyroMag) || (SendQuat))
  {
    static uint32_t counter = 0;
    static ble_manager_inertial_axes_t x_axes = {0, 0, 0};
    static ble_manager_inertial_axes_t g_axes = {0, 0, 0};
    static ble_manager_inertial_axes_t m_axes = {0, 0, 0};
    static ble_motion_sensor_axes_t q_axes = {0, 0, 0};
    /* Update emulated Acceleration, Gyroscope and Sensor Fusion data */
    Set_Random_Motion_Values(&x_axes, &g_axes, &m_axes, &q_axes, counter);
    if (SendAccGyroMag)
    {
      SendAccGyroMag = 0;

      if (paired == TRUE)
      {
        /* Send Values only if we are paired with the Device */
        ble_acc_gyro_mag_update(&x_axes, &g_axes, &m_axes);
      }
    }
    if (SendQuat)
    {
      SendQuat = 0;
      if (paired == TRUE)
      {
        /* Send Values only if we are paired with the Device */
        ble_sensor_fusion_update(&q_axes, 1);
      }
    }
    counter++;
    if (counter == 40)
    {
      counter = 0;
      Reset_Motion_Values(&x_axes, &g_axes, &m_axes, &q_axes);
    }
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
    static int32_t BleConnInfoWrittenOnNFC = 0;
    if (!BleConnInfoWrittenOnNFC)
    {
      BleConnInfoWrittenOnNFC = 1;
      BSP_LED_On(LED_YELLOW);
      /* Write BLE Pairing Information */
      BLE_Pairing_ST25DV();
      BSP_LED_Off(LED_YELLOW);
    }
    else
    {
      /* Clear the Secure DB if we are not connected */
      if (connected == FALSE)
      {
        ClearSecureDB();
        BleConnInfoWrittenOnNFC = 0;
      }
    }
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

/**
  * @brief  Set random values for all environmental sensor data
  *
  * @param  float pointer to temperature data
  * @param  float pointer to pressure data
  * @retval None
  */
void Set_Random_Environmental_Values(float *data_t, float *data_p)
{
  *data_t = 27.0 + ((uint64_t)rand() * 5) / RAND_MAX; /* T sensor emulation */
  *data_p = 1000.0 + ((uint64_t)rand() * 80) / RAND_MAX; /* P sensor emulation */
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

#ifdef STBOX1_ENABLE_START_BIP
  TIM_Beep_Init();
#endif /* STBOX1_ENABLE_START_BIP */

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

#ifdef STBOX1_ENABLE_START_BIP
  TIM_Beep_DeInit();
#endif /* STBOX1_ENABLE_START_BIP */
  InitTimers();

  /* Update the Current Fw ID saved in flash if it's neceessary */
  UpdateCurrFlashBankFwIdBoardName(STBOX1C_BLUEST_SDK_FW_ID, NULL);
}

/**
  * @brief  Print Bunner
  * @param  None
  * @retval None
  */
static void PrintInfo(void)
{
  STBOX1_PRINTF("\r\nSTMicroelectronics %s:\r\n"
                "\tVersion %c.%c.%c\r\n"
                "\tSTEVAL-MKBOXPRO Rev %c board"
                "\r\n",
                STBOX1_PACKAGENAME,
                STBOX1_VERSION_MAJOR, STBOX1_VERSION_MINOR, STBOX1_VERSION_PATCH,
                'C');
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

#ifdef STBOX1_ENABLE_START_BIP
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
    BLEDUALPROG_TIMx_CLK_ENABLE();
    /**TIM1 GPIO Configuration
    PE13     ------> TIM1_CH3
      */
    GPIO_InitStruct.Pin = SPKR_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = BLEDUALPROG_GPIO_AF1_TIMx;
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
static void TIM_Beep_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

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
static void TIM_Beep_DeInit(void)
{
  /* Reset the Timer Used for making the Beep*/
  HAL_GPIO_DeInit(SPKR_GPIO_PORT, SPKR_PIN);
  BLEDUALPROG_TIMx_FORCE_RESET();
  HAL_Delay(10);
  BLEDUALPROG_TIMx_RELEASE_RESET();
  HAL_Delay(10);
}

/**
  * @brief  Send Battery Info Data (Voltage/Current/Soc) to BLE
  * @param  uint32_t freq
  * @param  uint16_t time
  * @retval None
  */
static void beep(uint32_t freq, uint16_t time)
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
#endif /* STBOX1_ENABLE_START_BIP */

/**
  * @brief  Initialize Timers
  * @param  None
  * @retval None
  */
static void InitTimers(void)
{
  uint32_t uwPrescalerValue;

  /* Timer Output Compare Configuration Structure declaration */
  TIM_OC_InitTypeDef sConfig;

  /* Compute the prescaler value to counter clock equal to 10000 Hz */
  uwPrescalerValue = (uint32_t)((SystemCoreClock / 10000) - 1);

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

  sConfig.Pulse = STBOX1_UPDATE_ENV;
  if (HAL_TIM_OC_ConfigChannel(&TIM_CC_HANDLE, &sConfig, TIM_CHANNEL_2) != HAL_OK)
  {
    /* Configuration Error */
    STBOX1_Error_Handler(STBOX1_ERROR_HW_INIT, __FILE__, __LINE__);
  }

  sConfig.Pulse = STBOX1_UPDATE_INV;
  if (HAL_TIM_OC_ConfigChannel(&TIM_CC_HANDLE, &sConfig, TIM_CHANNEL_3) != HAL_OK)
  {
    /* Configuration Error */
    STBOX1_Error_Handler(STBOX1_ERROR_HW_INIT, __FILE__, __LINE__);
  }
}

/**
  * @brief  Set random values for all motion sensor data
  *
  * @param  ble_manager_inertial_axes_t *x_axes Acc Value
  * @param  ble_manager_inertial_axes_t *g_axes Gyro Value
  * @param  ble_manager_inertial_axes_t * m_axes Mag Value
  * @param  ble_motion_sensor_axes_t *q_axes  Quaternion Value
  * @param  uint32_t counter for changing the rotation direction
  * @retval None
  */
static void Set_Random_Motion_Values(ble_manager_inertial_axes_t *x_axes,
                                     ble_manager_inertial_axes_t *g_axes,
                                     ble_manager_inertial_axes_t *m_axes,
                                     ble_motion_sensor_axes_t *q_axes,
                                     uint32_t cnt)
{
  /* Update Acceleration, Gyroscope and Sensor Fusion data */
  if (cnt < 20)
  {
    x_axes->axis_x += (10  + ((uint64_t)rand() * 3 * cnt) / RAND_MAX);
    x_axes->axis_y += -(10  + ((uint64_t)rand() * 5 * cnt) / RAND_MAX);
    x_axes->axis_z += (10  + ((uint64_t)rand() * 7 * cnt) / RAND_MAX);
    g_axes->axis_x += (100 + ((uint64_t)rand() * 2 * cnt) / RAND_MAX);
    g_axes->axis_y += -(100 + ((uint64_t)rand() * 4 * cnt) / RAND_MAX);
    g_axes->axis_z += (100 + ((uint64_t)rand() * 6 * cnt) / RAND_MAX);
    m_axes->axis_x += (3  + ((uint64_t)rand() * 3 * cnt) / RAND_MAX);
    m_axes->axis_y += -(3  + ((uint64_t)rand() * 4 * cnt) / RAND_MAX);
    m_axes->axis_z += (3  + ((uint64_t)rand() * 5 * cnt) / RAND_MAX);

    q_axes->axis_x -= (100  + ((uint64_t)rand() * 3 * cnt) / RAND_MAX);
    q_axes->axis_y += (100  + ((uint64_t)rand() * 5 * cnt) / RAND_MAX);
    q_axes->axis_z -= (100  + ((uint64_t)rand() * 7 * cnt) / RAND_MAX);
  }
  else
  {
    x_axes->axis_x += -(10  + ((uint64_t)rand() * 3 * cnt) / RAND_MAX);
    x_axes->axis_y += (10  + ((uint64_t)rand() * 5 * cnt) / RAND_MAX);
    x_axes->axis_z += -(10  + ((uint64_t)rand() * 7 * cnt) / RAND_MAX);
    g_axes->axis_x += -(100 + ((uint64_t)rand() * 2 * cnt) / RAND_MAX);
    g_axes->axis_y += (100 + ((uint64_t)rand() * 4 * cnt) / RAND_MAX);
    g_axes->axis_z += -(100 + ((uint64_t)rand() * 6 * cnt) / RAND_MAX);
    m_axes->axis_x += -(3  + ((uint64_t)rand() * 7 * cnt) / RAND_MAX);
    m_axes->axis_y += (3  + ((uint64_t)rand() * 9 * cnt) / RAND_MAX);
    m_axes->axis_z += -(3  + ((uint64_t)rand() * 3 * cnt) / RAND_MAX);

    q_axes->axis_x += (200 + ((uint64_t)rand() * 7 * cnt) / RAND_MAX);
    q_axes->axis_y -= (150 + ((uint64_t)rand() * 3 * cnt) / RAND_MAX);
    q_axes->axis_z += (10  + ((uint64_t)rand() * 5 * cnt) / RAND_MAX);
  }
}

/**
  * @brief  Reset values for all motion sensor data
  *
  * @param  ble_manager_inertial_axes_t *x_axes Acc Value
  * @param  ble_manager_inertial_axes_t *g_axes Gyro Value
  * @param  ble_manager_inertial_axes_t * m_axes Mag Value
  * @param  ble_motion_sensor_axes_t *q_axes  Quaternion Value
  * @retval None
  */
static void Reset_Motion_Values(ble_manager_inertial_axes_t *x_axes,
                                ble_manager_inertial_axes_t *g_axes,
                                ble_manager_inertial_axes_t *m_axes,
                                ble_motion_sensor_axes_t *q_axes)
{
  x_axes->axis_x = (x_axes->axis_x) % 2000 == 0 ? -x_axes->axis_x : 10;
  x_axes->axis_y = (x_axes->axis_y) % 2000 == 0 ? -x_axes->axis_y : -10;
  x_axes->axis_z = (x_axes->axis_z) % 2000 == 0 ? -x_axes->axis_z : 10;
  g_axes->axis_x = (g_axes->axis_x) % 2000 == 0 ? -g_axes->axis_x : 100;
  g_axes->axis_y = (g_axes->axis_y) % 2000 == 0 ? -g_axes->axis_y : -100;
  g_axes->axis_z = (g_axes->axis_z) % 2000 == 0 ? -g_axes->axis_z : 100;
  m_axes->axis_x = (g_axes->axis_x) % 2000 == 0 ? -m_axes->axis_x : 3;
  m_axes->axis_y = (g_axes->axis_y) % 2000 == 0 ? -m_axes->axis_y : -3;
  m_axes->axis_z = (g_axes->axis_z) % 2000 == 0 ? -m_axes->axis_z : 3;
  q_axes->axis_x = -q_axes->axis_x;
  q_axes->axis_y = -q_axes->axis_y;
  q_axes->axis_z = -q_axes->axis_z;
}

/**
  * @brief  write one URI on NDEF
  * @param  Nne
  * @retval None
  */
void NDEF_URI_Init(void)
{
  sURI_Info URI;

  /* Init ST25DV driver */
  while (BSP_NFCTAG_Init(BSP_NFCTAG_INSTANCE) != NFCTAG_OK);

  /* Reset Mailbox enable to allow write to EEPROM */
  BSP_NFCTAG_ResetMBEN_Dyn(BSP_NFCTAG_INSTANCE);

  NfcTag_SelectProtocol(NFCTAG_TYPE5);

  /* Check if no NDEF detected, init mem in Tag Type 5 */
  if (NfcType5_NDEFDetection() != NDEF_OK)
  {
    CCFileStruct.MagicNumber = NFCT5_MAGICNUMBER_E1_CCFILE;
    CCFileStruct.Version = NFCT5_VERSION_V1_0;
    CCFileStruct.MemorySize = (ST25DV_MAX_SIZE / 8) & 0xFF;
    CCFileStruct.TT5Tag = 0x05;
    /* Init of the Type Tag 5 component (M24LR) */
    while (NfcType5_TT5Init() != NFCTAG_OK);
  }

  /* Prepare URI NDEF message content */
  strcpy(URI.protocol, URI_ID_0x01_STRING);
  strcpy(URI.URI_Message, "st.com");
  strcpy(URI.Information, "\0");

  /* Write NDEF to EEPROM */
  while (NDEF_WriteURI(&URI) != NDEF_OK);

  STBOX1_PRINTF("Written on NFC Uri=st.com\r\n");
}

/** @brief Write the NDEF values for NFC BLE pairing
  * @param None
  * @retval None
  */
static void BLE_Pairing_ST25DV(void)
{

  sURI_Info URI;
  /* Prepare URI NDEF message content */
  strcpy(URI.protocol, "\0");
  sprintf(URI.URI_Message, "stapplication://connect?Pin=%ld&Add=%02x:%02x:%02x:%02x:%02x:%02x",
          ble_stack_value.secure_pin,
          ble_stack_value.ble_mac_address[5],
          ble_stack_value.ble_mac_address[4],
          ble_stack_value.ble_mac_address[3],
          ble_stack_value.ble_mac_address[2],
          ble_stack_value.ble_mac_address[1],
          ble_stack_value.ble_mac_address[0]);
  strcpy(URI.Information, "\0");
  /* Write NDEF to EEPROM */
  while (NDEF_WriteURI(&URI) != NDEF_OK);
  STBOX1_PRINTF("ST25DV Bluetooth NDEF Table written\r\n");
}

/**
  * @brief  Clear Secure Database
  * @param  None
  * @retval None
  */
static void ClearSecureDB(void)
{
  ble_status_t ret = aci_gap_clear_security_db();

  if (ret != BLE_STATUS_SUCCESS)
  {
    STBOX1_PRINTF("aci_gap_clear_security_db failed:0x%02x\r\n", ret);
  }
  else
  {
    STBOX1_PRINTF("aci_gap_clear_security_db\r\n");
  }

  /* Led Blinking */
  if (CurrentActiveBank == 2)
  {
    MCR_HEART_BIT2();
  }
  else
  {
    MCR_HEART_BIT();
  }
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
    BlinkLed = 1;
  }

  /* TIM1_CH2 toggling with frequency = 2Hz */
  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
  {
    uhCapture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
    /* Set the Capture Compare Register value */
    __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_2, (uhCapture + STBOX1_UPDATE_ENV));
    SendEnv = 1;
  }

  /* TIM1_CH3 toggling with frequency = 20Hz */
  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)
  {
    uhCapture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
    /* Set the Capture Compare Register value */
    __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_3, (uhCapture + STBOX1_UPDATE_INV));
    if (W2ST_CHECK_CONNECTION(W2ST_CONNECT_QUAT_EVENT))
    {
      SendQuat = 1;
    }
    else
    {
      SendAccGyroMag = 1;
    }
  }
}

