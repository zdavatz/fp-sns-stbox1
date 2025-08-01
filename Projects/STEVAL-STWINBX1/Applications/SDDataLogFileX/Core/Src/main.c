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
 * - STEVAL-STWINBX1 (STWIN.box) evaluation board that contains the following components:
 *   - MEMS sensor devices: IIS2DLPC, IIS2MDC, IIS3DWB, ISM330DHCX, IIS2ICLX, ILPS22QS, STTS22H
 *   - analog (IMP23ABSU) and digital (IMP34DT05) microphones
 *   - dynamic NFC tag: ST25DV64
 *   - BlueNRG-2 Bluetooth Low Energy System On Chip

 * <b>Example Application</b>

 * The Example application provides one example of one simple SD Data logger Pressing the User Button is possible to start/stop the logger For each log session, the board saves:
 *   - one .wav file that it’s the output of the digital microphone
 *   - one .csv file with the sensors logged at 100Hz
 * 
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
#include <stdio.h>
#include "STWIN.box_env_sensors.h"
#include "STWIN.box_motion_sensors.h"

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ICACHE_Init(void);
static void PrintInfo(void);
static void InitMemsSensors(void);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{


  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();



  /* Configure the system clock */
  SystemClock_Config();

#if (FX_STM32_SD_INIT == 0)
  BSP_SD_Init(FX_STM32_SD_INSTANCE);
#endif

  MX_GPIO_Init();
  
  /* Initialize ICache */
  MX_ICACHE_Init();
  
  /* Enable VDDA */
  BSP_Enable_LDO();

  BSP_LED_Init(LED_GREEN);
  BSP_LED_Init(LED_ORANGE);
  /* Enable Button in Interrupt mode */
  BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_EXTI);
  
  /* Init UART for printf */
  BSP_COM_Init(COM1);
  
  /* Print Banner */
  PrintInfo();
  
  /* Init Mems Sensors */
  InitMemsSensors();
  

  MX_ThreadX_Init();
  /* Infinite loop */
  while (1)
  {

  }
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
              "\tSTM32U585AI-STWIN.box board"
                "\r\n",
                STBOX1_PACKAGENAME,
                STBOX1_VERSION_MAJOR,STBOX1_VERSION_MINOR,STBOX1_VERSION_PATCH);
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

#if defined (__ARMCC_VERSION)
extern UART_HandleTypeDef hcom_uart[];
/** @brief fgetc call for standard input implementation
 * @param f File pointer
 * @retval Character acquired from standard input
 */
int fgetc(FILE *f)
{
  int ch;
	(void)HAL_UART_Receive(&hcom_uart[COM1], (uint8_t*) &ch, 1, COM_POLL_TIMEOUT);
	return ch;
}
#endif

/**
* @brief  Init Mems Sensors
* @param  None
* @retval None
*/
static void InitMemsSensors(void)
{ 
  /* Magneto */
  if(BSP_MOTION_SENSOR_Init(IIS2MDC_0, MOTION_MAGNETO)==BSP_ERROR_NONE) {
    if(BSP_MOTION_SENSOR_SetOutputDataRate(IIS2MDC_0, MOTION_MAGNETO, IIS2MDC_MAG_ODR)==BSP_ERROR_NONE) {
      if(BSP_MOTION_SENSOR_SetFullScale(IIS2MDC_0, MOTION_MAGNETO, IIS2MDC_MAG_FS)==BSP_ERROR_NONE) {
        STBOX1_PRINTF("IIS2MDC OK\r\n");
      } else {
        STBOX1_PRINTF("Error: IIS2MDC KO\r\n");
      }
    }else {
      STBOX1_PRINTF("Error: IIS2MDC KO\r\n");
    }
  } else {
    STBOX1_PRINTF("Error: IIS2MDC KO\r\n");
  }
  
  /* Acc/Gyro */
  if(BSP_MOTION_SENSOR_Init(ISM330DHCX_0, MOTION_ACCELERO | MOTION_GYRO)==BSP_ERROR_NONE) {
    if(BSP_MOTION_SENSOR_SetOutputDataRate(ISM330DHCX_0, MOTION_ACCELERO, ISM330DHCX_ACC_ODR)==BSP_ERROR_NONE) {
      if(BSP_MOTION_SENSOR_SetFullScale(ISM330DHCX_0, MOTION_ACCELERO, ISM330DHCX_ACC_FS)==BSP_ERROR_NONE) {
        if(BSP_MOTION_SENSOR_SetOutputDataRate(ISM330DHCX_0, MOTION_GYRO, ISM330DHCX_GYRO_ODR)==BSP_ERROR_NONE) {
          if(BSP_MOTION_SENSOR_SetFullScale(ISM330DHCX_0, MOTION_GYRO, ISM330DHCX_GYRO_FS)==BSP_ERROR_NONE) {
            STBOX1_PRINTF("ISM330DHCX OK\r\n");
          } else {
            STBOX1_PRINTF("Error: ISM330DHCX KO 1\r\n");
          }
        } else {
          STBOX1_PRINTF("Error: ISM330DHCX KO 2\r\n");
        }
      } else {
        STBOX1_PRINTF("Error: ISM330DHCX KO 3\r\n");
      }
    } else {
      STBOX1_PRINTF("Error: ISM330DHCX KO 4\r\n");
    }
  } else {
    STBOX1_PRINTF("Error: ISM330DHCX KO 5\r\n");
  }
  
  /* Pressure */
  if(BSP_ENV_SENSOR_Init(ILPS22QS_0, ENV_PRESSURE)==BSP_ERROR_NONE) {
    if(BSP_ENV_SENSOR_SetOutputDataRate(ILPS22QS_0, ENV_PRESSURE, ILPS22QS_ODR)==BSP_ERROR_NONE) {
      STBOX1_PRINTF("ILPS22QS OK\r\n");
    } else {
      STBOX1_PRINTF("Error: ILPS22QS KO\r\n");
    }
  } else {
    STBOX1_PRINTF("Error: ILPS22QS KO\r\n");
  }
  
  /* Temperature 2 */
  if(BSP_ENV_SENSOR_Init(STTS22H_0, ENV_TEMPERATURE)==BSP_ERROR_NONE) {
    if(BSP_ENV_SENSOR_SetOutputDataRate(STTS22H_0, ENV_TEMPERATURE, STTS22H_ODR)==BSP_ERROR_NONE) {
      STBOX1_PRINTF("STTS22H OK\r\n");
    } else {
      STBOX1_PRINTF("Error: STTS22H KO\r\n");
    }
  } else {
    STBOX1_PRINTF("Error: STTS22H KO\r\n");
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};


  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) /// fabio usa PWR_REGULATOR_VOLTAGE_SCALE2
  {
    Error_Handler(__FILE__,__LINE__);
  }

  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI|RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;


  // RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_4;//RCC_MSIRANGE_4; ///4 davide; /// era 0
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI; /// era RCC_PLLSOURCE_HSE
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;

  /// come davide (HSI)
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 80;
  RCC_OscInitStruct.PLL.PLLP = 8;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;

  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_0;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler(__FILE__,__LINE__);
  }
  /** Initializes the CPU, AHB and APB busses clocks
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
  __HAL_RCC_PWR_CLK_DISABLE();
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

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

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
void Error_Handler(char *File,int32_t Line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  BSP_LED_Off(LED_GREEN);
  STBOX1_PRINTF("Error at %d at %s\r\n",Line,File);
  while (1)
  {
    BSP_LED_Toggle(LED_ORANGE);
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
