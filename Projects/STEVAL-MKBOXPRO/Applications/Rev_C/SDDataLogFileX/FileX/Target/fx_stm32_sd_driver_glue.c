/**
  ******************************************************************************
  * @file    fx_stm32_sd_driver_glue.c
  * @author  System Research & Applications Team - Catania Lab.
  * @version V2.1.0
  * @date    20-May-2025
  * @brief   fx stm32 sd driver glue implementation
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
#include "fx_stm32_sd_driver.h"
#include "tx_api.h"  /* v52: tx_thread_sleep for the yielding get_status */

/* USER CODE BEGIN  0 */
TX_SEMAPHORE transfer_semaphore;
/* USER CODE END  0 */

/**
* @brief Initializes the SD IP instance
* @param uINT Instance SD instance to initialize
* @retval 0 on success error value otherwise
*/
INT fx_stm32_sd_init(UINT instance)
{
  INT ret = 0;
/* USER CODE BEGIN  FX_SD_INIT */
if (BSP_SD_Init(instance) == BSP_ERROR_NONE)
  {
    ret = 0;
  }
  else
  {
    ret = 1;
  }
/* USER CODE END  FX_SD_INIT */

  return ret;
}

/**
* @brief Deinitializes the SD IP instance
* @param uINT Instance SD instance to deinitialize
* @retval 0 on success error value otherwise
*/
INT fx_stm32_sd_deinit(UINT instance)
{
  INT ret = 0;
/* USER CODE BEGIN  FX_SD_DEINIT */
UNUSED(instance);
/* USER CODE END  FX_SD_DEINIT */

  return ret;
}

/**
* @brief Check the SD IP status.
* @param uINT Instance SD instance to check
* @retval 0 when ready 1 when busy
*/
INT fx_stm32_sd_get_status(UINT instance)
{
  INT ret = 0;
/* USER CODE BEGIN  GET_STATUS */
  if (BSP_SD_GetCardState(instance) == BSP_ERROR_NONE)
  {
    ret = 0;
  }
  else
  {
    ret = 1;
    /* v52: yield 10 ms when the card reports busy. The caller
       (`check_sd_status` in fx_stm32_sd_driver.c) loops for up to 10 s
       waiting for ret==0, but its outer loop has no `tx_thread_sleep`
       — so without this yield, fx_thread monopolises the CPU for the
       full 10 s timeout of a stuck card on Peter's reservebox. The
       sleep here lets gps_thread / ble_thread / other ready threads
       run during the wait, and lowers SD-busy-poll rate from
       ~MHz-spin to a sane 100 Hz. Cost: marginally slower recovery on
       a card that's about to be ready in microseconds. */
    tx_thread_sleep(1U);
  }
/* USER CODE END  GET_STATUS */
  return ret;
}

/**
* @brief Read Data from the SD device into a buffer.
* @param uINT *Buffer buffer into which the data is to be read.
* @param uINT StartBlock the first block to start reading from.
* @param uINT NbrOfBlocks total number of blocks to read.
* @retval 0 on success error code otherwise
*/
INT fx_stm32_sd_read_blocks(UINT instance, UINT *buffer, UINT start_block, UINT total_blocks)
{
  INT ret = 0;
/* USER CODE BEGIN  READ_BLOCKS */
 if (BSP_SD_ReadBlocks_DMA(instance, (uint32_t *)buffer, start_block, total_blocks) != BSP_ERROR_NONE)
  {
    ret = -1;
  }
  else
  {
  }
/* USER CODE END  READ_BLOCKS */
  return ret;
}
/**
* @brief Write data buffer into the SD device.
* @param uINT *Buffer buffer .to write into the SD device.
* @param uINT StartBlock the first block to start writing from.
* @param uINT NbrOfBlocks total number of blocks to write.
* @retval 0 on success error code otherwise
*/

INT fx_stm32_sd_write_blocks(UINT instance, UINT *buffer, UINT start_block, UINT total_blocks)
{
  INT ret = 0;
/* USER CODE BEGIN  WRITE_BLOCKS */
 if (BSP_SD_WriteBlocks_DMA(instance, (uint32_t *)buffer, start_block, total_blocks) != BSP_ERROR_NONE)
  {
    ret = -1;
  }
  else
  {
  }
/* USER CODE END  WRITE_BLOCKS */
  return ret;

}

/* USER CODE BEGIN  1 */
void BSP_SD_WriteCpltCallback(uint32_t instance)
{
  (void)instance;
  tx_semaphore_put(&transfer_semaphore);
}

/**
* @brief SD DMA Rx Transfer completed callbacks
* @param Instance the sd instance
* @retval None
*/
void BSP_SD_ReadCpltCallback(uint32_t instance)
{
  (void)instance;
  tx_semaphore_put(&transfer_semaphore);
}

/* v62: SDMMC error/abort recovery — replace upstream HAL/BSP weak stubs
   that did NOT release transfer_semaphore on error. Without these
   overrides, an SDMMC IRQ that fires DCRCFAIL / DTIMEOUT / TXUNDERR /
   RXOVERR / IDMABTC error results in the HAL_SD_IRQHandler calling
   HAL_SD_ErrorCallback() (or the BSP's SD_AbortCallback → empty
   BSP_SD_AbortCallback), which does NOTHING. fx_thread then waits the
   full 10-second tx_semaphore_get timeout before returning FX_IO_ERROR.
   On a marginally-supplied SD card (Peter's 3.3V-modded reservebox)
   where errors can stack across the multiple writes inside a single
   fx_media_flush, this compounds to 30-60 seconds of perceived hang.
   With these callbacks the semaphore wakes immediately on error, and
   fx_thread can return cleanly within ms. Issue #12. */

/* Overrides upstream stm32u5xx_hal_sd.c's __weak HAL_SD_ErrorCallback. */
void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
  (void)hsd;
  tx_semaphore_put(&transfer_semaphore);
}

/* Overrides BSP's __weak BSP_SD_AbortCallback. The HAL → BSP chain is:
   HAL_SD_AbortCallback → BSP_SD_AbortCallback (= our override below). */
void BSP_SD_AbortCallback(uint32_t instance)
{
  (void)instance;
  tx_semaphore_put(&transfer_semaphore);
}
/* USER CODE END  1 */
