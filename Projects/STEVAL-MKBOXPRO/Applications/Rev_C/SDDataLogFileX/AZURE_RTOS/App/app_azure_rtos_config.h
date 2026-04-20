/**
  ******************************************************************************
  * @file    app_azure_rtos_config.h
  * @author  System Research & Applications Team - Catania Lab.
  * @version V2.1.0
  * @date    20-May-2025
  * @brief   app_azure_rtos config header file
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef APP_AZURE_RTOS_CONFIG_H
#define APP_AZURE_RTOS_CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif

/* Exported constants --------------------------------------------------------*/
/* Using static memory allocation via threadX Byte memory pools */

#define USE_MEMORY_POOL_ALLOCATION               1

#define TX_APP_MEM_POOL_SIZE                     1024

/* 14 KB covers fx_thread (4 KB) + read_thread (4 KB) + gps_thread (2 KB) +
 * MessageQueue (~400 B) + tx_byte_pool overhead. 10 KB was too tight after
 * adding gps_thread — tx_byte_allocate returned NO_MEMORY and main's boot
 * stage 3 was the last visible step. */
#define FX_APP_MEM_POOL_SIZE                     (1024*14)

#ifdef __cplusplus
}
#endif

#endif /* APP_AZURE_RTOS_CONFIG_H */
