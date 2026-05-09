/**
  ******************************************************************************
  * @file    main.c
  * @author  System Research & Applications Team - Catania Lab.
  * @version V2.1.0
  * @date    20-May-2025
  * @brief   main.c file
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

/**
 *
 * @page SDDataLogFileX Simple Data Log on SD Card
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

 * <b>Example Application</b>

 * The Example application provides one example of one simple SD Data logger Pressing the User Button is possible to start/stop the logger For each log session, the board saves:
 *   - one .wav file that it’s the output of the digital microphone
 *   - one .csv file with the sensors logged at 100Hz
 * 
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_cdc.h"
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "SensorTileBoxPro.h"
#include "SensorTileBoxPro_env_sensors.h"
#include "SensorTileBoxPro_motion_sensors.h"
#include "gps_nmea.h"
#include "buzzer.h"
#include "ble_sync.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define STTS22H_ODR 1.0f /* ODR = 1.0Hz */
#define LSM6DSV16X_ACC_ODR 120.0f /* ODR = 120Hz */
#define LSM6DSV16X_ACC_FS 4 /* FS = 4g */
#define LSM6DSV16X_GYRO_ODR 120.0f /* ODR = 120Hz */
#define LSM6DSV16X_GYRO_FS 500 /* FS = 500dps (17.5 mdps/LSB, 4x finer than 2000dps) */
#define LPS22DF_ODR 25.0f /* ODR = 25.0Hz */
#define LIS2MDL_MAG_ODR 100.0f /* ODR = 100Hz */
#define LIS2MDL_MAG_FS 50 /* FS = 50gauss */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Snapshot of RCC->CSR reset-reason flags, captured before the flags are
   cleared. Read by app_filex.c::ErrorLog_Open() to stamp the boot marker
   with the reset cause (POR / BOR / SOFTWARE / PIN / IWDG / WWDG / LPWR /
   OBL). Distinguishes "user turned it on" from "brown-out killed us mid
   session" from "we crashed" — the 53s-abort signature we couldn't tell
   apart before. */
uint32_t BootResetCsr;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_ICACHE_Init(void);
static void PrintInfo(void);
static void InitMemsSensors(void);
static void BootStageBlink(uint8_t count);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* Snapshot the reset-reason flags before anything else touches RCC->CSR,
     then clear them so the *next* reset's flags are fresh. The flags are
     sticky across resets (except POR clears them), so at this moment they
     describe what woke us up this boot. app_filex.c::ErrorLog_Open()
     renders BootResetCsr into a human-readable "reset: BOR" / "reset:
     SOFTWARE" / etc. line in the error log. */
  BootResetCsr = RCC->CSR;
  __HAL_RCC_CLEAR_RESET_FLAGS();
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

#if (FX_STM32_SD_INIT == 0)
  BSP_SD_Init(FX_STM32_SD_INSTANCE);
#endif
  /* USER CODE END SysInit */

  /* Initialize ICache */
  MX_ICACHE_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE BEGIN SysInit */
  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_RED);
  /* RED is activated by default */
  BSP_LED_Off(LED_RED);

  /* Boot-progress indicator: 1 green blink = we got past clock+icache+LED init. */
  BootStageBlink(1);

  /* Enable Button in Interrupt mode */
  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);

  /* Init UART4 for the u-blox MAX-M10S GPS module (PA0=TX, PA1=RX, 38400 8N1).
     Replaces the former BSP_COM_Init(COM1) debug-printf role.
     STBOX1_PRINTF() is compiled out (see stbox1_config.h). */
  GPS_Init();
  BootStageBlink(2);

  /* Print Banner (no-op when STBOX1_ENABLE_PRINTF is undefined) */
  PrintInfo();

  /* Init Mems Sensors */
  InitMemsSensors();
  BootStageBlink(3);

  /* Audible "boot complete" indicator on the piezo (PE13 / TIM1_CH3).
     Two short ascending tones (~240 ms total) right before ThreadX takes
     over. Useful in the field where the LED isn't always visible. */
  Buzzer_Init();
  Buzzer_BootDone();

  /* USB CDC ACM virtual COM port. No-op when STBOX1_ENABLE_USB_CDC=0; with
     the flag on, brings up OTG_FS so __io_putchar (and therefore printf
     and STBOX1_PRINTF) lands on USB. Service thread is spawned later from
     App_ThreadX_Init(). Hardware init must happen here (pre-RTOS) because
     the OTG_FS NVIC line begins firing as soon as the host plugs in.
     Distinctive blink right after init so the field tester can tell from
     the LED whether USB hardware bring-up succeeded:
       1 long green flash (~600 ms) = tud_rhport_init OK
       7 quick red bursts            = tud_rhport_init FAILED (the dwc2
                                       Synopsys-ID check failed, usually
                                       clock or power) */
  if (UsbCdc_Init()) {
    BSP_LED_On(LED_GREEN);  HAL_Delay(600); BSP_LED_Off(LED_GREEN);
  } else {
    DiagBlinkRed(7);
  }

  /* USER CODE END 2 */

  MX_ThreadX_Init();
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
* @brief  Print Bunner
* @param  None
* @retval None
*/
static void PrintInfo(void)
{
  /* serial console clear screen */
  STBOX1_PRINTF("\033[2J");
  
  /* serial console cursor to home */
  STBOX1_PRINTF("\033[H");
  
  STBOX1_PRINTF("\r\nSTMicroelectronics %s:\r\n"
            "\tVersion %c.%c.%c\r\n"
              "\tSTM32U585AI-SensorTile.box-Pro (%c) board"
                "\r\n",
                STBOX1_PACKAGENAME,
                STBOX1_VERSION_MAJOR,STBOX1_VERSION_MINOR,STBOX1_VERSION_PATCH,'C');
  STBOX1_PRINTF("\t(HAL %ld.%ld.%ld_%ld)\r\n"
            "\tCompiled %s %s"
#if defined (__IAR_SYSTEMS_ICC__)
              " (IAR)\r\n",
#elif defined (__ARMCC_VERSION)
              " (KEIL)\r\n",
#elif defined (__GNUC__)
              " (STM32CubeIDE)\r\n",
#endif
              HAL_GetHalVersion() >>24,
              (HAL_GetHalVersion() >>16)&0xFF,
              (HAL_GetHalVersion() >> 8)&0xFF,
              HAL_GetHalVersion()      &0xFF,
              __DATE__,__TIME__);
}

/**
* @brief  Diagnostic red-blink helper. Used inside InitMemsSensors to
*         pinpoint which sensor init hangs. Pattern: short ~80 ms
*         on/off, with a 400 ms gap after the burst so consecutive
*         bursts are visually distinct from the green BootStageBlink.
*/
void DiagBlinkRed(uint8_t n)
{
  for (uint8_t i = 0; i < n; i++) {
    BSP_LED_On(LED_RED);
    HAL_Delay(80);
    BSP_LED_Off(LED_RED);
    HAL_Delay(80);
  }
  HAL_Delay(400);
}

/**
* @brief  Init Mems Sensors
* @param  None
* @retval None
*/
static void InitMemsSensors(void)
{
   GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Diagnostic step 1: entered InitMemsSensors */
  DiagBlinkRed(1);

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOI_CLK_ENABLE();

  /*Configure GPIO pin Output Level 5-> BSP_LSM6DSV16X_CS_PIN */
  HAL_GPIO_WritePin(GPIOI, GPIO_PIN_5, GPIO_PIN_SET);

  /*Configure GPIO pins : PI5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
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
#endif
  
  /* Diagnostic step 2: about to init LIS2MDL magnetometer (I2C2) */
  DiagBlinkRed(2);

  /* Magneto */
  if(BSP_MOTION_SENSOR_Init(LIS2MDL_0, MOTION_MAGNETO)==BSP_ERROR_NONE) {
    if(BSP_MOTION_SENSOR_SetOutputDataRate(LIS2MDL_0, MOTION_MAGNETO, LIS2MDL_MAG_ODR)==BSP_ERROR_NONE) {
      if(BSP_MOTION_SENSOR_SetFullScale(LIS2MDL_0, MOTION_MAGNETO, LIS2MDL_MAG_FS)==BSP_ERROR_NONE) {
        STBOX1_PRINTF("LIS2MDL_0 OK\r\n");
      } else {
        STBOX1_PRINTF("Error: LIS2MDL_0 KO\r\n");
      }
    }else {
      STBOX1_PRINTF("Error: LIS2MDL_0 KO\r\n");
    }
  } else {
    STBOX1_PRINTF("Error: LIS2MDL_0 KO\r\n");
  }
  
  /* Diagnostic step 3: about to init LSM6DSV16X acc/gyro (SPI) */
  DiagBlinkRed(3);

  /* Acc/Gyro */
  if(BSP_MOTION_SENSOR_Init(LSM6DSV16X_0, MOTION_ACCELERO | MOTION_GYRO)==BSP_ERROR_NONE) {
    if(BSP_MOTION_SENSOR_SetOutputDataRate(LSM6DSV16X_0, MOTION_ACCELERO, LSM6DSV16X_ACC_ODR)==BSP_ERROR_NONE) {
      if(BSP_MOTION_SENSOR_SetFullScale(LSM6DSV16X_0, MOTION_ACCELERO, LSM6DSV16X_ACC_FS)==BSP_ERROR_NONE) {
        if(BSP_MOTION_SENSOR_SetOutputDataRate(LSM6DSV16X_0, MOTION_GYRO, LSM6DSV16X_GYRO_ODR)==BSP_ERROR_NONE) {
          if(BSP_MOTION_SENSOR_SetFullScale(LSM6DSV16X_0, MOTION_GYRO, LSM6DSV16X_GYRO_FS)==BSP_ERROR_NONE) {
            STBOX1_PRINTF("LSM6DSV16X_0 OK\r\n");
          } else {
            STBOX1_PRINTF("Error: LSM6DSV16X_0 KO\r\n");
          }
        } else {
          STBOX1_PRINTF("Error: LSM6DSV16X_0 KO\r\n");
        }
      } else {
        STBOX1_PRINTF("Error: LSM6DSV16X_0 KO\r\n");
      }
    } else {
      STBOX1_PRINTF("Error: LSM6DSV16X_0 KO\r\n");
    }
  } else {
    STBOX1_PRINTF("Error: LSM6DSV16X_0 KO\r\n");
  }
  
  /* Diagnostic step 4: about to init LPS22DF pressure sensor (I2C2) */
  DiagBlinkRed(4);

  /* Pressure */
  if(BSP_ENV_SENSOR_Init(LPS22DF_0, ENV_PRESSURE)==BSP_ERROR_NONE) {
    if(BSP_ENV_SENSOR_SetOutputDataRate(LPS22DF_0, ENV_PRESSURE, LPS22DF_ODR)==BSP_ERROR_NONE) {
      STBOX1_PRINTF("LPS22DF_0 OK\r\n");
    } else {
      STBOX1_PRINTF("Error: LPS22DF_0 KO\r\n");
    }
  } else {
    STBOX1_PRINTF("Error: LPS22DF_0 KO\r\n");
  }
  
  /* Diagnostic step 5: about to init STTS22H temperature sensor (I2C2) */
  DiagBlinkRed(5);

  /* Temperature  */
  if(BSP_ENV_SENSOR_Init(STTS22H_0, ENV_TEMPERATURE)==BSP_ERROR_NONE) {
    if(BSP_ENV_SENSOR_SetOutputDataRate(STTS22H_0, ENV_TEMPERATURE, STTS22H_ODR)==BSP_ERROR_NONE) {
      STBOX1_PRINTF("STTS22H_0 OK\r\n");
    } else {
      STBOX1_PRINTF("Error: STTS22H_0 KO\r\n");
    }
  } else {
    STBOX1_PRINTF("Error: STTS22H_0 KO\r\n");
  }

  /* Diagnostic step 6: all four sensor inits returned (or skipped) */
  DiagBlinkRed(6);
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler(__FILE__,__LINE__);
  }

  /** Initializes the CPU, AHB and APB buses clocks using HSI (internal 16 MHz).
   *  HSE was unreliable on battery-power boot — the external crystal sometimes
   *  did not start within HSE_STARTUP_TIMEOUT, pushing the firmware into
   *  Error_Handler before main() could turn off the power-on-default red LED.
   *  HSI is always available and gives the same 160 MHz sysclk with the same
   *  PLL multipliers. */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI
                              |RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.LSIDiv = RCC_LSI_DIV1;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 1;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler(__FILE__,__LINE__);
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler(__FILE__,__LINE__);
  }
  
    /** Initializes the common periph clock
  */
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_MDF1|RCC_PERIPHCLK_ADF1
                              |RCC_PERIPHCLK_ADCDAC;
  PeriphClkInit.Mdf1ClockSelection = RCC_MDF1CLKSOURCE_PLL3;
  PeriphClkInit.Adf1ClockSelection = RCC_ADF1CLKSOURCE_PLL3;
  PeriphClkInit.AdcDacClockSelection = RCC_ADCDACCLKSOURCE_PLL2;
  PeriphClkInit.PLL3.PLL3Source = RCC_PLLSOURCE_HSI;
  PeriphClkInit.PLL3.PLL3M = 2;
  PeriphClkInit.PLL3.PLL3N = 48;
  PeriphClkInit.PLL3.PLL3P = 2;
  PeriphClkInit.PLL3.PLL3Q = 25;
  PeriphClkInit.PLL3.PLL3R = 2;
  PeriphClkInit.PLL3.PLL3RGE = RCC_PLLVCIRANGE_1;
  PeriphClkInit.PLL3.PLL3FRACN = 0;
  PeriphClkInit.PLL3.PLL3ClockOut = RCC_PLL3_DIVQ;
  PeriphClkInit.PLL2.PLL2Source = RCC_PLLSOURCE_HSI;
  PeriphClkInit.PLL2.PLL2M = 2;
  PeriphClkInit.PLL2.PLL2N = 48;
  PeriphClkInit.PLL2.PLL2P = 2;
  PeriphClkInit.PLL2.PLL2Q = 7;
  PeriphClkInit.PLL2.PLL2R = 25;
  PeriphClkInit.PLL2.PLL2RGE = RCC_PLLVCIRANGE_1;
  PeriphClkInit.PLL2.PLL2FRACN = 0;
  PeriphClkInit.PLL2.PLL2ClockOut = RCC_PLL2_DIVR;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler(__FILE__,__LINE__);
  }
}

/**
  * @brief ICACHE Initialization Function
  * @param None
  * @retval None
  */
static void MX_ICACHE_Init(void)
{

  /* USER CODE BEGIN ICACHE_Init 0 */

  /* USER CODE END ICACHE_Init 0 */

  /* USER CODE BEGIN ICACHE_Init 1 */

  /* USER CODE END ICACHE_Init 1 */
  /** Enable instruction cache in 1-way (direct mapped cache)
  */
  if (HAL_ICACHE_ConfigAssociativityMode(ICACHE_1WAY) != HAL_OK)
  {
    Error_Handler(__FILE__,__LINE__);
  }
  if (HAL_ICACHE_Enable() != HAL_OK)
  {
    Error_Handler(__FILE__,__LINE__);
  }
  /* USER CODE BEGIN ICACHE_Init 2 */

  /* USER CODE END ICACHE_Init 2 */

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param char *File Name
  * @param int32_t Line number
  * @retval None
  */
/**
  * @brief  Boot-progress indicator. Blinks green N times, then a 700 ms pause.
  *         Used to diagnose where the firmware stops when the red LED comes on
  *         without any other feedback (STBOX1_PRINTF is compiled out).
  * @param  count  Number of green blinks (1..n).
  */
static void BootStageBlink(uint8_t count)
{
  for (uint8_t i = 0; i < count; i++)
  {
    BSP_LED_On(LED_GREEN);
    HAL_Delay(120);
    BSP_LED_Off(LED_GREEN);
    HAL_Delay(120);
  }
  HAL_Delay(700);
}

void Error_Handler(char *File,int32_t Line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  BSP_LED_Off(LED_GREEN);
  STBOX1_PRINTF("Error at %d at %s\r\n",Line,File);

  /* Write error to SD card log */
  {
    char err_msg[256];
    sprintf(err_msg, "FATAL: %s:%ld", File, (long)Line);
    ErrorLog_Write(err_msg);
    ErrorLog_Close();
  }

  while (1)
  {
    BSP_LED_Toggle(LED_RED);
    HAL_Delay(200);
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
