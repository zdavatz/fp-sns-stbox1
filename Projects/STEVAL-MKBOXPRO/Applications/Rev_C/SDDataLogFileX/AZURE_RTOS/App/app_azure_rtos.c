/**
  ******************************************************************************
  * @file    app_azure_rtos.c
  * @author  System Research & Applications Team - Catania Lab.
  * @version V2.1.0
  * @date    20-May-2025
  * @brief   app_azure_rtos application implementation file
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
#include "app_azure_rtos.h"
#include "SensorTileBoxPro.h"
#include "stm32u5xx_hal.h"

/* Loop the red LED forever in a count-pause-count pattern. Used as a
   visible halt diagnostic in the four error paths in tx_application_define
   below — without this, halt-before-tx_kernel_enter shows up as "3 green
   boot blinks then dark", indistinguishable from a hang anywhere else.
   Each pattern: `count` 200 ms on/off red blinks, then 1.2 s pause, repeat.
   Patterns: 2 = tx_app_byte_pool failed, 4 = App_ThreadX_Init failed
   (BLE thread alloc/create), 6 = fx_app_byte_pool failed, 8 = MX_FileX_Init
   failed. */
static void halt_red_morse(uint8_t count)
{
  for (;;) {
    for (uint8_t i = 0; i < count; i++) {
      BSP_LED_On(LED_RED);
      HAL_Delay(200);
      BSP_LED_Off(LED_RED);
      HAL_Delay(200);
    }
    HAL_Delay(1200);
  }
}

/* Private variables ---------------------------------------------------------*/

#if (USE_MEMORY_POOL_ALLOCATION == 1)
/* USER CODE BEGIN TX_Pool_Buffer */
/* USER CODE END TX_Pool_Buffer */
static UCHAR tx_byte_pool_buffer[TX_APP_MEM_POOL_SIZE];
static TX_BYTE_POOL tx_app_byte_pool;

/* USER CODE BEGIN FX_Pool_Buffer */
/* USER CODE END FX_Pool_Buffer */
static UCHAR  fx_byte_pool_buffer[FX_APP_MEM_POOL_SIZE];
static TX_BYTE_POOL fx_app_byte_pool;

#endif /* (USE_MEMORY_POOL_ALLOCATION == 1) */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief  Define the initial system.
  * @param  first_unused_memory : Pointer to the first unused memory
  * @retval None
  */
VOID tx_application_define(VOID *first_unused_memory)
{
  /* USER CODE BEGIN  tx_application_define_1*/

  /* USER CODE END  tx_application_define_1 */
#if (USE_MEMORY_POOL_ALLOCATION == 1)
  UINT status = TX_SUCCESS;
  VOID *memory_ptr;

  if (tx_byte_pool_create(&tx_app_byte_pool, "Tx App memory pool", tx_byte_pool_buffer,
                          TX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
  {
    /* USER CODE BEGIN TX_Byte_Pool_Error */
    halt_red_morse(2);
    /* USER CODE END TX_Byte_Pool_Error */
  }
  else
  {
    /* USER CODE BEGIN TX_Byte_Pool_Success */

    /* USER CODE END TX_Byte_Pool_Success */

    memory_ptr = (VOID *)&tx_app_byte_pool;
    status = App_ThreadX_Init(memory_ptr);
    if (status != TX_SUCCESS)
    {
      /* USER CODE BEGIN  App_ThreadX_Init_Error */
      halt_red_morse(4);
      /* USER CODE END  App_ThreadX_Init_Error */
    }
    /* USER CODE BEGIN  App_ThreadX_Init_Success */

    /* USER CODE END  App_ThreadX_Init_Success */

  }
  if (tx_byte_pool_create(&fx_app_byte_pool, "Fx App memory pool", fx_byte_pool_buffer,
                          FX_APP_MEM_POOL_SIZE) != TX_SUCCESS)
  {
    /* USER CODE BEGIN FX_Byte_Pool_Error */
    halt_red_morse(6);
    /* USER CODE END FX_Byte_Pool_Error */
  }
  else
  {
    /* USER CODE BEGIN FX_Byte_Pool_Success */

    /* USER CODE END FX_Byte_Pool_Success */

    memory_ptr = (VOID *)&fx_app_byte_pool;
    status = MX_FileX_Init(memory_ptr);
    if (status != FX_SUCCESS)
    {
      /* USER CODE BEGIN  MX_FileX_Init_Error */
      halt_red_morse(8);
      /* USER CODE END  MX_FileX_Init_Error */
    }
    /* USER CODE BEGIN  MX_FileX_Init_Success */

    /* USER CODE END  MX_FileX_Init_Success */
  }

#else /* (USE_MEMORY_POOL_ALLOCATION == 1) */
  /*
   * Using dynamic memory allocation requires to apply some changes to the linker file.
   * ThreadX needs to pass a pointer to the first free memory location in RAM to the tx_application_define() function,
   * using the "first_unused_memory" argument.
   * This require changes in the linker files to expose this memory location.
   * For EWARM add the following section into the .icf file:
       place in RAM_region    { last section FREE_MEM };
   * For MDK-ARM
       - either define the RW_IRAM1 region in the ".sct" file
       - or modify the line below in "tx_low_level_initilize.s to match the memory region being used
          LDR r1, =|Image$$RW_IRAM1$$ZI$$Limit|

   * For STM32CubeIDE add the following section into the .ld file:
       ._threadx_heap :
         {
            . = ALIGN(8);
            __RAM_segment_used_end__ = .;
            . = . + 64K;
            . = ALIGN(8);
          } >RAM_D1 AT> RAM_D1
      * The simplest way to provide memory for ThreadX is to define a new section, see ._threadx_heap above.
      * In the example above the ThreadX heap size is set to 64KBytes.
      * The ._threadx_heap must be located between the .bss and the ._user_heap_stack sections in the linker script.
      * Caution: Make sure that ThreadX does not need more than the provided heap memory (64KBytes in this example).
      * Read more in STM32CubeIDE User Guide, chapter: "Linker script".

   * The "tx_initialize_low_level.s" should be also modified to enable the "USE_DYNAMIC_MEMORY_ALLOCATION" flag.
   */

  /* USER CODE BEGIN DYNAMIC_MEM_ALLOC */
  (void)first_unused_memory;
  /* USER CODE END DYNAMIC_MEM_ALLOC */
#endif /* (USE_MEMORY_POOL_ALLOCATION == 1) */

}
