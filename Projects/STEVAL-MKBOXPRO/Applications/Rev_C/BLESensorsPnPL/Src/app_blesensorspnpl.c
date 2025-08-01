/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_blesensorspnpl.c
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
  * @page BLESensorsPnPL BLE Firmware example using PnP-Like messages
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
  * This example shows how it's possible to customize the demo present on ST BLE Sensors Application
  * using the PnP-Like Messages
  * This example is compatible with the Firmware Over the Air Update (FoTA)
  *
  * This example must be used with the related ST BLE Sensor Android/iOS application available on Play/itune store
  * (Version 5.2.0 or higher), in order to read the sent information by Bluetooth Low Energy protocol
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "app_blesensorspnpl.h"
#include "stm32wb07_06_conf.h"
#include "ble_function.h"
#include "ota.h"
#include "stbox1_config.h"
#include "SensorTileBoxPro_env_sensors.h"
#include "SensorTileBoxPro_motion_sensors.h"
#include "SensorTileBoxPro_gg.h"
#include "steval_mkboxpro.h"

#include "PnPLCompManager.h"
#include "i_control.h"
#include "configuration_pn_p_l.h"
#include "control_pn_p_l.h"
#include "environmental_pn_p_l.h"
#include "inertial_pn_p_l.h"
#include "device_information_pn_p_l.h"

/* Exported variables --------------------------------------------------------*/
uint16_t ConnectionHandle = 0;
/* Memory Bank used */
int32_t CurrentActiveBank = 0;

void *HandleGGComponent;

/* Identify if battery is present or not */
uint8_t BatteryPresent = 0;

STBOX1_Acc_t CurrentAccType = STBOX1_ACC_LSM6DSV16X;

#define TIMERS_PERIOD 65535

#define ENV_PULSE_1 10000
#define ENV_PULSE_2 1000
#define ENV_PULSE_3 500

#define INER_PULSE_1 1000
#define INER_PULSE_2 500
#define INER_PULSE_3 333

/* Private variables ---------------------------------------------------------*/
static volatile uint32_t user_button_pressed = 0;
/* Enable led blinking */
static volatile uint32_t BlinkLed = 0;

/* Enable to send battery info data via Bluetooth */
static volatile uint32_t SendBatteryInfo = 0;

/* Enable to send inertial data via Bluetooth */
static volatile uint32_t SendAccGyroMag = 0;

/* Enable to send environmental data via Bluetooth */
static volatile uint32_t SendEnv = 0;

static IPnPLComponent_t *pConfigurationPnPLObj;
static IPnPLComponent_t *pControlPnPLObj;
static IPnPLComponent_t *pEnvironmentalPnPLObj;
static IPnPLComponent_t *pInertialPnPLObj;
static IPnPLComponent_t *pDeviceInformationPnPLObj;
static IControl_t iControl;

/* Private Functions prototypes ----------------------------------------------*/
static void User_Init(void);
static void PrintInfo(void);

static void SendBatteryInfoData(void);

static void InitTimers(void);
static void InitMemsSensors(void);

/* Exported Functions --------------------------------------------------------*/
/**
  * @brief  The application entry point.
  * @retval none
  */
void MX_BLESensorsPnPL_Init(void)
{
  /* Set a random seed */
  srand(HAL_GetTick());

  User_Init();

  STBOX1_PRINTF("\033[2J"); /* serial console clear screen */
  STBOX1_PRINTF("\033[H");  /* serial console cursor to home */
  PrintInfo();

  BSP_LED_On(LED_BLUE);

  /* Init Mems Sensors */
  InitMemsSensors();
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

  /* PnP-L Components Allocation */
  pConfigurationPnPLObj = Configuration_PnPLAlloc();
  pControlPnPLObj = Control_PnPLAlloc();
  pEnvironmentalPnPLObj = Environmental_PnPLAlloc();
  pInertialPnPLObj = Inertial_PnPLAlloc();
  pDeviceInformationPnPLObj = Deviceinformation_PnPLAlloc();

  /* Init&Add PnP-L Components */
  Configuration_PnPLInit(pConfigurationPnPLObj);
  Control_PnPLInit(pControlPnPLObj, &iControl);
  Environmental_PnPLInit(pEnvironmentalPnPLObj);
  Inertial_PnPLInit(pInertialPnPLObj);
  Deviceinformation_PnPLInit(pDeviceInformationPnPLObj);

  PnPLSetBOARDID(BLE_MANAGER_SENSOR_TILE_BOX_PRO_C_PLATFORM);
  PnPLSetFWID(STBOX1C_BLUEST_SDK_FW_ID);

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
void MX_BLESensorsPnPL_Process(void)
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

  /*  Update sensor value */
  if (SendEnv)
  {
    SendEnv = 0;

    /* Send Environmental Values */
    {
      float Temp;
      float Pressure;

      BSP_ENV_SENSOR_GetValue(STTS22H_0, ENV_TEMPERATURE, &Temp);
      BSP_ENV_SENSOR_GetValue(LPS22DF_0, ENV_PRESSURE, &Pressure);
      ble_environmental_update((int32_t)(Pressure * 100), 0 /* Not Used */, (int16_t)(Temp * 10), 0 /* Not Used */);
    }
  }

  if (SendAccGyroMag)
  {
    SendAccGyroMag = 0;

    /* Send Inertial Sensor Values */
    {
      ble_manager_inertial_axes_t x_axes;
      ble_manager_inertial_axes_t g_axes;
      ble_manager_inertial_axes_t m_axes;

      if (CurrentAccType == STBOX1_ACC_LSM6DSV16X)
      {
        BSP_MOTION_SENSOR_GetAxes(ACCELERO_INSTANCE, MOTION_ACCELERO, (BSP_MOTION_SENSOR_Axes_t *)&x_axes);
      }
      else
      {
        BSP_MOTION_SENSOR_GetAxes(LIS2DU12_0, MOTION_ACCELERO, (BSP_MOTION_SENSOR_Axes_t *)&x_axes);
      }

      BSP_MOTION_SENSOR_GetAxes(GYRO_INSTANCE, MOTION_GYRO, (BSP_MOTION_SENSOR_Axes_t *)&g_axes);
      BSP_MOTION_SENSOR_GetAxes(MAGNETO_INSTANCE, MOTION_MAGNETO, (BSP_MOTION_SENSOR_Axes_t *)&m_axes);

      ble_acc_gyro_mag_update(&x_axes, &g_axes, &m_axes);
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
  }

  /* Blinking the Led */
  if (BlinkLed)
  {
    BlinkLed = 0;
    BSP_LED_Toggle(LED_GREEN);
  }

  /* Check if we need to send a Chunk of Data for PnPL */
  if (JSON_string_command_wTP != NULL)
  {
    PnPLikeSendChunckData();
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

  switch (CurrentEnvUpdateEnumValue)
  {
    case 1:
      sConfig.Pulse = ENV_PULSE_1;
      break;
    case 10:
      sConfig.Pulse = ENV_PULSE_2;
      break;
    case 20:
      sConfig.Pulse = ENV_PULSE_3;
      break;
  }
  if (HAL_TIM_OC_ConfigChannel(&TIM_CC_HANDLE, &sConfig, TIM_CHANNEL_2) != HAL_OK)
  {
    /* Configuration Error */
    STBOX1_Error_Handler(STBOX1_ERROR_HW_INIT, __FILE__, __LINE__);
  }

  switch (CurrentInerUpdateEnumValue)
  {
    case 10:
      sConfig.Pulse = INER_PULSE_1;
      break;
    case 20:
      sConfig.Pulse = INER_PULSE_2;
      break;
    case 30:
      sConfig.Pulse = INER_PULSE_3;
      break;
  }
  if (HAL_TIM_OC_ConfigChannel(&TIM_CC_HANDLE, &sConfig, TIM_CHANNEL_3) != HAL_OK)
  {
    /* Configuration Error */
    STBOX1_Error_Handler(STBOX1_ERROR_HW_INIT, __FILE__, __LINE__);
  }
}

/**
  * @brief  Init Magneto and Interrupt for Acc/Gyro
  * @param  None
  * @retval None
  */
static void InitMemsSensors(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOI_CLK_ENABLE();

  /*Configure GPIO pin Output Level 5-> BSP_LSM6DSV16X_CS_PIN 7-> BSP_LIS2DU12_CS_PIN*/
  HAL_GPIO_WritePin(GPIOI, GPIO_PIN_5 | GPIO_PIN_7, GPIO_PIN_SET);

  /*Configure GPIO pins : PI5 PI7 */
  GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

#ifndef ALL_SENSORS_I2C
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOI, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pins : PI0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
#endif /* ALL_SENSORS_I2C */

  /* Magneto */
  if (BSP_MOTION_SENSOR_Init(MAGNETO_INSTANCE, MOTION_MAGNETO) == BSP_ERROR_NONE)
  {
    if (BSP_MOTION_SENSOR_SetOutputDataRate(MAGNETO_INSTANCE, MOTION_MAGNETO, MAG_ODR) == BSP_ERROR_NONE)
    {
      if (BSP_MOTION_SENSOR_SetFullScale(MAGNETO_INSTANCE, MOTION_MAGNETO, MAG_FS) == BSP_ERROR_NONE)
      {
        STBOX1_PRINTF("Init Magneto Sensor OK\r\n");
        if (BSP_MOTION_SENSOR_Enable(MAGNETO_INSTANCE, MOTION_MAGNETO) == BSP_ERROR_NONE)
        {
          STBOX1_PRINTF("Enable Magneto Sensor OK\r\n");
        }
      }
      else
      {
        STBOX1_PRINTF("Error: Magneto Sensor Set Full Scale KO\r\n");
      }
    }
    else
    {
      STBOX1_PRINTF("Error: Magneto Sensor Set Output Data Rate KO\r\n");
    }
  }
  else
  {
    STBOX1_PRINTF("Error: Magneto Sensor Init KO\r\n");
  }

  /* Acc/Gyro */
  if (BSP_MOTION_SENSOR_Init(ACCELERO_INSTANCE, MOTION_ACCELERO | MOTION_GYRO) == BSP_ERROR_NONE)
  {
    if (BSP_MOTION_SENSOR_SetOutputDataRate(ACCELERO_INSTANCE, MOTION_ACCELERO, ACC_ODR) == BSP_ERROR_NONE)
    {
      if (BSP_MOTION_SENSOR_SetFullScale(ACCELERO_INSTANCE, MOTION_ACCELERO, ACC_FS) == BSP_ERROR_NONE)
      {
        if (BSP_MOTION_SENSOR_SetOutputDataRate(GYRO_INSTANCE, MOTION_GYRO, GYRO_ODR) == BSP_ERROR_NONE)
        {
          if (BSP_MOTION_SENSOR_SetFullScale(GYRO_INSTANCE, MOTION_GYRO, GYRO_FS) == BSP_ERROR_NONE)
          {
            STBOX1_PRINTF("Init Accelero and Gyro Sensor OK\r\n");
            if (BSP_MOTION_SENSOR_Enable(ACCELERO_INSTANCE, MOTION_ACCELERO) == BSP_ERROR_NONE)
            {
              STBOX1_PRINTF("Enable Accelero Sensor OK\r\n");
            }

            if (BSP_MOTION_SENSOR_Enable(GYRO_INSTANCE, MOTION_GYRO) == BSP_ERROR_NONE)
            {
              STBOX1_PRINTF("Enable Gyro Sensor OK\r\n");
            }
          }
          else
          {
            STBOX1_PRINTF("Error: Gyro Sensor Set Full Scale KO\r\n");
          }
        }
        else
        {
          STBOX1_PRINTF("Error: Gyro Set Output Data Rate KO\r\n");
        }
      }
      else
      {
        STBOX1_PRINTF("Error: Accelero Sensor Set Full Scale KO\r\n");
      }
    }
    else
    {
      STBOX1_PRINTF("Error: Accelero Set Output Data Rate KO\r\n");
    }
  }
  else
  {
    STBOX1_PRINTF("Error: Accelero init KO\r\n");
  }

  /* Acc2 */
  if (BSP_MOTION_SENSOR_Init(ACCELERO_INSTANCE_2, MOTION_ACCELERO) == BSP_ERROR_NONE)
  {
    if (BSP_MOTION_SENSOR_SetOutputDataRate(ACCELERO_INSTANCE_2, MOTION_ACCELERO, ACC_ODR_2) == BSP_ERROR_NONE)
    {
      if (BSP_MOTION_SENSOR_SetFullScale(ACCELERO_INSTANCE_2, MOTION_ACCELERO, ACC_FS_2) == BSP_ERROR_NONE)
      {
        STBOX1_PRINTF("Init Accelero 2 Sensor OK\r\n");
        if (BSP_MOTION_SENSOR_Enable(ACCELERO_INSTANCE_2, MOTION_ACCELERO) == BSP_ERROR_NONE)
        {
          STBOX1_PRINTF("Enable Accelero 2 Sensor OK\r\n");
        }
      }
      else
      {
        STBOX1_PRINTF("Error: Accelero 2 Set Full Scale KO\r\n");
      }
    }
    else
    {
      STBOX1_PRINTF("Error: Accelero 2 Set Output Data Rate KO\r\n");
    }
  }
  else
  {
    STBOX1_PRINTF("Error: Accelero 2 Sensor Init KO\r\n");
  }

  /* Pressure */
  if (BSP_ENV_SENSOR_Init(PRESSURE_INSTANCE, ENV_PRESSURE) == BSP_ERROR_NONE)
  {
    if (BSP_ENV_SENSOR_SetOutputDataRate(PRESSURE_INSTANCE, ENV_PRESSURE, PRESS_ODR) == BSP_ERROR_NONE)
    {
      STBOX1_PRINTF("Init Pressure Sensor OK\r\n");
      if (BSP_ENV_SENSOR_Enable(PRESSURE_INSTANCE, ENV_PRESSURE) == BSP_ERROR_NONE)
      {
        STBOX1_PRINTF("Enable Pressure Sensor OK\r\n");
      }
    }
    else
    {
      STBOX1_PRINTF("Error: Pressure Sensor Set Output Data Rate KO\r\n");
    }
  }
  else
  {
    STBOX1_PRINTF("Error: Pressure Sensor Init KO\r\n");
  }

  /* Temperature */
  if (BSP_ENV_SENSOR_Init(TEMPERATURE_INSTANCE, ENV_TEMPERATURE) == BSP_ERROR_NONE)
  {
    if (BSP_ENV_SENSOR_SetOutputDataRate(TEMPERATURE_INSTANCE, ENV_TEMPERATURE, TEMP_ODR) == BSP_ERROR_NONE)
    {
      STBOX1_PRINTF("Init Temperature Sensor OK\r\n");
      if (BSP_ENV_SENSOR_Enable(TEMPERATURE_INSTANCE, ENV_TEMPERATURE) == BSP_ERROR_NONE)
      {
        STBOX1_PRINTF("Enable Temperature Sensor OK\r\n");
      }
    }
    else
    {
      STBOX1_PRINTF("Error: Temperature Sensor Set Output Data Rate KO\r\n");
    }
  }
  else
  {
    STBOX1_PRINTF("Error: Temperature Sensor Init KO\r\n");
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

  /* TIM1_CH2 toggling with frequency = 2Hz */
  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
  {
    uhCapture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
    /* Set the Capture Compare Register value */
    switch (CurrentEnvUpdateEnumValue)
    {
      case 1:
        __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_2, (uhCapture + 10000));
        break;
      case 10:
        __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_2, (uhCapture + 1000));
        break;
      case 20:
        __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_2, (uhCapture + 500));
        break;
    }
    SendEnv = 1;
  }

  /* TIM1_CH3 toggling with frequency = 20Hz */
  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)
  {
    uhCapture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
    /* Set the Capture Compare Register value */
    switch (CurrentInerUpdateEnumValue)
    {
      case 10:
        __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_3, (uhCapture + 1000));
        break;
      case 20:
        __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_3, (uhCapture + 500));
        break;
      case 30:
        __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_3, (uhCapture + 333));
        break;
    }
    SendAccGyroMag = 1;
  }
}

/**
  * @brief This function provides accurate delay (in milliseconds) based
  *        on variable incremented.
  * @note This is a user implementation using WFI state
  * @param Delay: specifies the delay time length, in milliseconds.
  * @retval None
  */
void HAL_Delay(__IO uint32_t Delay)
{
  uint32_t tickstart = 0;
  tickstart = HAL_GetTick();
  while ((HAL_GetTick() - tickstart) < Delay)
  {
    __WFI();
  }
}
