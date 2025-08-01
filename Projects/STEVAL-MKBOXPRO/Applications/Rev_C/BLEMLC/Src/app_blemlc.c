/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_blemlc.c
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
  * @page BLEMLC BLE Firmware example for Machine Learning Core and Finite State Machine
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
  * This example explains how to use Machine Learning Core and the Finite State Machine
  * This example is compatible with the Firmware Over the Air Update (FoTA)
  *
  * This example must be used with the related ST BLE Sensor Android/iOS application available on Play/itune store
  * (Version 5.2.0 or higher), in order to read the sent information by Bluetooth Low Energy protocol
  *
  */

/* Includes ------------------------------------------------------------------*/
#include "app_blemlc.h"
#include "stm32wb07_06_conf.h"
#include "ble_function.h"
#include "ota.h"
#include "stbox1_config.h"
#include "SensorTileBoxPro_motion_sensors.h"
#include "SensorTileBoxPro_motion_sensors_ex.h"
/* Default program for Machine Learning Core */
#include "lsm6dsv16x_activity_4classes.h"
/* Default program for Finite State Machine */
#include "lsm6dsv16x_four_d.h"
#include "steval_mkboxpro.h"

/* Exported variables --------------------------------------------------------*/
uint16_t ConnectionHandle = 0;
/* Memory Bank used */
int32_t CurrentActiveBank = 0;

void *HandleGGComponent;

EXTI_HandleTypeDef H_EXTI_INT_LSM6DSV16X = {.Line = INT_LSM6DSV16X_EXTI_LINE};

ble_ar_output_t ActivityCode = BLE_AR_ERROR;

/* Custom UCF files for MLC and FSM */
ucf_line_t *MLCCustomUCFFile = NULL;
uint32_t MLCCustomUCFFileLength = 0;
ucf_line_t *FSMCustomUCFFile = NULL;
uint32_t FSMCustomUCFFileLength = 0;

/* Labels for Custom UCF Files form MLC and FSM */
char *MLCCustomLabels = NULL;
char *FSMCustomLabels = NULL;
uint32_t MLCCustomLabelsLength = 0;
uint32_t FSMCustomLabelsLength = 0;

#define TIMERS_PERIOD 65535

/* Private typedef -----------------------------------------------------------*/
/* This array Maps the output of .ucf filt to the activities knowed by ST BLE Sensor application:
 *  - 1 : Stationary
 *  - 4 : Walking
 *  - 8 : Running
 *  - 12: Driving
 */
ble_ar_output_t MappingToHAR_ouput_t[13] =
{
  BLE_AR_NOACTIVITY,  /* 0 */
  BLE_AR_STATIONARY,  /* 1 */
  BLE_AR_ERROR,       /* 2 */
  BLE_AR_ERROR,       /* 3 */
  BLE_AR_WALKING,     /* 4 */
  BLE_AR_ERROR,       /* 5 */
  BLE_AR_ERROR,       /* 6 */
  BLE_AR_ERROR,       /* 7 */
  BLE_AR_JOGGING,     /* 8 */
  BLE_AR_ERROR,       /* 9 */
  BLE_AR_ERROR,       /* 10 */
  BLE_AR_ERROR,       /* 11 */
  BLE_AR_DRIVING     /* 12 */
};

/* Private variables ---------------------------------------------------------*/
static volatile uint32_t user_button_pressed = 0;
/* Enable led blinking */
static volatile uint32_t BlinkLed = 0;

static volatile uint32_t MEMSInterrupt = 0;

/* Enable to send inertial data via Bluetooth */
static volatile uint32_t SendAccGyroMag = 0;

/* Private Functions prototypes ----------------------------------------------*/
static void User_Init(void);
static void PrintInfo(void);

static void InitTimers(void);
static void InitMemsSensors(void);
static void MEMSCallback(void);
static void hexti_callback(void);

/* Exported Functions --------------------------------------------------------*/
/**
  * @brief  The application entry point.
  * @retval none
  */
void MX_BLEMLC_Init(void)
{
  /* Set a random seed */
  srand(HAL_GetTick());

  User_Init();

  STBOX1_PRINTF("\033[2J"); /* serial console clear screen */
  STBOX1_PRINTF("\033[H");  /* serial console cursor to home */
  PrintInfo();

  BSP_LED_On(LED_BLUE);

  /* Init Magneto and Interrupt for Acc/Gyro */
  InitMemsSensors();

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
void MX_BLEMLC_Process(void)
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

  if (SendAccGyroMag)
  {
    SendAccGyroMag = 0;

    /* Send Inertial Sensor Values */
    {
      ble_manager_inertial_axes_t x_axes;
      ble_manager_inertial_axes_t g_axes;
      ble_manager_inertial_axes_t m_axes;

      BSP_MOTION_SENSOR_GetAxes(ACCELERO_INSTANCE, MOTION_ACCELERO, (BSP_MOTION_SENSOR_Axes_t *)&x_axes);
      BSP_MOTION_SENSOR_GetAxes(GYRO_INSTANCE, MOTION_GYRO, (BSP_MOTION_SENSOR_Axes_t *)&g_axes);
      BSP_MOTION_SENSOR_GetAxes(MAGNETO_INSTANCE, MOTION_MAGNETO, (BSP_MOTION_SENSOR_Axes_t *)&m_axes);

      ble_acc_gyro_mag_update(&x_axes, &g_axes, &m_axes);
    }
  }

  /* Handle the MEMS interrupt */
  if (MEMSInterrupt)
  {
    MEMSInterrupt = 0;
    MEMSCallback();
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

/** @brief Initialize the LSM6DSV16X MEMS Sensor
  * @param None
  * @retval None
  */
void InitAcc(void)
{
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
            STBOX1_PRINTF("ACCELERO_INSTANCE and GYRO_INSTANCE OK\r\n");
          }
          else
          {
            STBOX1_PRINTF("Error: GYRO_INSTANCE Set Full Scale KO\r\n");
          }
        }
        else
        {
          STBOX1_PRINTF("Error: GYRO_INSTANCE Set Output Data Rate KO\r\n");
        }
      }
      else
      {
        STBOX1_PRINTF("Error: ACCELERO_INSTANCE Set Full Scale KO\r\n");
      }
    }
    else
    {
      STBOX1_PRINTF("Error: ACCELERO_INSTANCE Set Output Data Rate KO\r\n");
    }
  }
  else
  {
    STBOX1_PRINTF("Error: ACCELERO_INSTANCE init KO\r\n");
  }
}

/** @brief DeInitialize the MEMS Sensor
  * @param None
  * @retval None
  */
void DeInit_Acc(void)
{
  /* DeInit Accelero */
  if (BSP_MOTION_SENSOR_DeInit(ACCELERO_INSTANCE) == BSP_ERROR_NONE)
  {
    STBOX1_PRINTF("OK Deinit ACCELERO_INSTANCE  Sensor\n\r");
  }
  else
  {
    STBOX1_PRINTF("Error Deinit ACCELERO_INSTANCE Sensor\n\r");
    STBOX1_Error_Handler(STBOX1_ERROR_SENSOR, __FILE__, __LINE__);
  }

}

/** @brief Initialize the MEMS Sensor for MLC
  * @param uint32_t UseCustomIfAvailableflag for Using or not the Custom UCF file
  * @retval None
  */
void InitAcc_MLC(uint32_t UseCustomIfAvailable)
{
  ucf_line_t *ProgramPointer;
  int32_t LineCounter;
  int32_t TotalNumberOfLine;
  int32_t RetValue;

  /* Init Accelero */
  if (BSP_MOTION_SENSOR_Init(ACCELERO_INSTANCE, MOTION_ACCELERO) == BSP_ERROR_NONE)
  {
    STBOX1_PRINTF("OK Init Accelero  Sensor\n\r");
  }
  else
  {
    STBOX1_PRINTF("Error Init Accelero Sensor\n\r");
    STBOX1_Error_Handler(STBOX1_ERROR_SENSOR, __FILE__, __LINE__);
  }

  /* Feed the program to Machine Learning Core */
  if ((UseCustomIfAvailable == 1) & (MLCCustomUCFFile != NULL))
  {
    ProgramPointer    = MLCCustomUCFFile;
    TotalNumberOfLine = MLCCustomUCFFileLength;
    STBOX1_PRINTF("-->Custom UCF Program for Accelero MLC\r\n");
  }
  else
  {
    /* Activity Recognition Default program */
    ProgramPointer = (ucf_line_t *)HAR_DSV16X_4classes;
    TotalNumberOfLine = sizeof(HAR_DSV16X_4classes) / sizeof(ucf_line_t);
    STBOX1_PRINTF("-->Activity Recognition for Accelero MLC\r\n");
    STBOX1_PRINTF("UCF Number Line=%ld\r\n", TotalNumberOfLine);

  }

  for (LineCounter = 0; LineCounter < TotalNumberOfLine; LineCounter++)
  {
    RetValue = BSP_MOTION_SENSOR_Write_Register(ACCELERO_INSTANCE,
                                                ProgramPointer[LineCounter].address,
                                                ProgramPointer[LineCounter].data);
    if (RetValue != BSP_ERROR_NONE)
    {
      STBOX1_PRINTF("Error loading the Program to Accelero [%ld]->%lx\n\r", LineCounter, RetValue);
      STBOX1_Error_Handler(STBOX1_ERROR_SENSOR, __FILE__, __LINE__);
    }
  }

  STBOX1_PRINTF("Program loaded inside the Accelero MLC\n\r");
}

/** @brief Initialize the MEMS Sensor for FSM
  * @param uint32_t UseCustomIfAvailable flag for Using or not the Custom UCF file
  * @retval None
  */
void InitAcc_FSM(uint32_t UseCustomIfAvailable)
{
  ucf_line_t *ProgramPointer;
  int32_t LineCounter;
  int32_t TotalNumberOfLine;
  int32_t RetValue;

  /* Init Accelero */
  if (BSP_MOTION_SENSOR_Init(ACCELERO_INSTANCE, MOTION_ACCELERO) == BSP_ERROR_NONE)
  {
    STBOX1_PRINTF("OK Init Accelero  Sensor\n\r");
  }
  else
  {
    STBOX1_PRINTF("Error Init Accelero Sensor\n\r");
    STBOX1_Error_Handler(STBOX1_ERROR_SENSOR, __FILE__, __LINE__);
  }

  /* Feed the program to FiniteStateMachine */
  if ((UseCustomIfAvailable == 1) & (FSMCustomUCFFile != NULL))
  {
    ProgramPointer    = FSMCustomUCFFile;
    TotalNumberOfLine = FSMCustomUCFFileLength;
    STBOX1_PRINTF("-->Custom UCF Program for Accelero FSM\r\n");
  }
  else
  {
    /* 4D position recognition Default program */
    ProgramPointer = (ucf_line_t *)lsm6dsv16x_four_d;
    TotalNumberOfLine = sizeof(lsm6dsv16x_four_d) / sizeof(ucf_line_t);
    STBOX1_PRINTF("-->4D position recognition for Accelero FSM\r\n");
    STBOX1_PRINTF("UCF Number Line=%ld\r\n", TotalNumberOfLine);
  }

  for (LineCounter = 0; LineCounter < TotalNumberOfLine; LineCounter++)
  {
    RetValue = BSP_MOTION_SENSOR_Write_Register(ACCELERO_INSTANCE,
                                                ProgramPointer[LineCounter].address,
                                                ProgramPointer[LineCounter].data);
    if (RetValue != BSP_ERROR_NONE)
    {
      STBOX1_PRINTF("Error loading the Program to Accelero [%ld]->%lx\n\r", LineCounter, RetValue);
      STBOX1_Error_Handler(STBOX1_ERROR_SENSOR, __FILE__, __LINE__);
    }
  }
  STBOX1_PRINTF("Program loaded inside the Accelero FSM\n\r");
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

  sConfig.Pulse = STBOX1_UPDATE_INV;
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

  {
    /* Enable interrupts from INT1 LSM6DSV16X  */
    GPIO_InitTypeDef GPIO_InitStruct;

    INT_LSM6DSV16X_GPIO_CLK_ENABLE();

    GPIO_InitStruct.Pin = INT_LSM6DSV16X_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(INT_LSM6DSV16X_GPIO_PORT, &GPIO_InitStruct);

    /* EXTI interrupt init*/
    HAL_EXTI_GetHandle(&H_EXTI_INT_LSM6DSV16X, INT_LSM6DSV16X_EXTI_LINE);
    HAL_EXTI_RegisterCallback(&H_EXTI_INT_LSM6DSV16X, HAL_EXTI_COMMON_CB_ID, hexti_callback);
    HAL_NVIC_SetPriority(INT_LSM6DSV16X_EXTI_IRQ_N, 5, 0);
    HAL_NVIC_EnableIRQ(INT_LSM6DSV16X_EXTI_IRQ_N);

    STBOX1_PRINTF("Enabled LSM6DSV16X INT1 Detection \n\r");
  }
}

/**
  * @brief  Callback Interrupt
  * @param  None
  * @retval None
  */
static void hexti_callback(void)
{
  MEMSInterrupt = 1;
}

/**
  * @brief  Callback for LSM6DS0X interrupt
  * @param  None
  * @retval None
  */
static void MEMSCallback(void)
{
  lsm6dsv16x_all_sources_t      status;
  uint8_t MLCStatus;
  uint8_t FSMStatus;

  STBOX1_PRINTF("MEMSCallback\n\r");

  lsm6dsv16x_all_sources_get(LSM6DSV16X_CONTEX, &status);

  MLCStatus = (status.mlc1) | (status.mlc2 << 1) | (status.mlc3 << 2) | (status.mlc4 << 3);

  FSMStatus = ((status.fsm1) | (status.fsm2 << 1) | (status.fsm3 << 2) | (status.fsm4 << 3) |
               (status.fsm5 << 4) | (status.fsm6 << 5) | (status.fsm7 << 6) | (status.fsm8 << 7));

  if (MLCStatus)
  {
    lsm6dsv16x_mlc_out_t mlc_out;
    lsm6dsv16x_mlc_out_get(LSM6DSV16X_CONTEX, &mlc_out);

    /*printf("MLC %d %d %d %d\r\n",mlc_out.mlc1_src,mlc_out.mlc2_src,mlc_out.mlc3_src,mlc_out.mlc4_src);*/

    /* Check if we need to update the MLC BLE char */
    if (W2ST_CHECK_CONNECTION(W2ST_CONNECT_MLC))
    {
      ble_machine_learning_core_update((uint8_t *) &mlc_out, &MLCStatus);
    }

    /* Check if we need to update the Activity Recognition BLE char */
    if (W2ST_CHECK_CONNECTION(W2ST_CONNECT_AR_EVENT))
    {
      if (status.mlc1)
      {
        ActivityCode = MappingToHAR_ouput_t[mlc_out.mlc1_src];
        if (ActivityCode != BLE_AR_ERROR)
        {
          ble_act_rec_update(ActivityCode, HAR_ALGO_IDX_NONE);
          if (ble_std_term_service == BLE_SERV_ENABLE)
          {
            bytes_to_write = sprintf((char *)buffer_to_write,
                                     "Rec ActivityCode %02X [%02X]\n",
                                     ActivityCode,
                                     mlc_out.mlc1_src);
            term_update(buffer_to_write, bytes_to_write);
          }
          else
          {
            STBOX1_PRINTF("Rec ActivityCode %02X [%02X]\r\n", ActivityCode, mlc_out.mlc1_src);
          }
        }
        else
        {
          if (ble_std_term_service == BLE_SERV_ENABLE)
          {
            bytes_to_write = sprintf((char *)buffer_to_write,
                                     "Wrong ActivityCode %02X [%02X]\n",
                                     ActivityCode,
                                     mlc_out.mlc1_src);
            term_update(buffer_to_write, bytes_to_write);
          }
          else
          {
            STBOX1_PRINTF("Wrong ActivityCode %02X [%02X]\r\n", ActivityCode, mlc_out.mlc1_src);
          }
        }
      }
    }
  }
  else if (FSMStatus)
  {
    lsm6dsv16x_fsm_out_t fsm_out;
    lsm6dsv16x_fsm_out_get(LSM6DSV16X_CONTEX, &fsm_out);

    /*printf("FSM %d %d %d %d %d %d %d %d\r\n",fsm_out.fsm_outs1,fsm_out.fsm_outs2,fsm_out.fsm_outs3,fsm_out.fsm_outs4,
           fsm_out.fsm_outs5,fsm_out.fsm_outs6,fsm_out.fsm_outs7,fsm_out.fsm_outs8);*/

    if (W2ST_CHECK_CONNECTION(W2ST_CONNECT_FSM))
    {
      ble_finite_state_machine_update((uint8_t *)&fsm_out, &FSMStatus,/* this is dummy */ &FSMStatus);
    }
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

  /* TIM1_CH3 toggling with frequency = 20Hz */
  if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)
  {
    uhCapture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
    /* Set the Capture Compare Register value */
    __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_3, (uhCapture + STBOX1_UPDATE_INV));
    SendAccGyroMag = 1;
  }
}

