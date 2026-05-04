/**
  ******************************************************************************
  * @file    app_filex.h
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    20-May-2025
  * @brief   FileX applicative header file
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
#ifndef __APP_FILEX_H__
#define __APP_FILEX_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "fx_api.h"
#include "fx_stm32_sd_driver.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
UINT MX_FileX_Init(VOID *memory_ptr);

/* USER CODE BEGIN EFP */
/* Posts a STOP_LOG message into the FileX writer's queue. Called from the
   BLE thread when the host sends a STOP_LOG opcode — mirrors what the
   user button does, but routes through a dedicated static MessageData_t
   so concurrent button + BLE stops don't share a buffer. */
void Ble_RequestStopLog(void);

/* Returns non-zero while any log file (Sens/Gps) is currently open for
   writing. The BUSY guard in ble_filesync rejects READ/DELETE on those
   files so the FAT doesn't get pulled out from under the writer. */
int  Ble_IsLoggingActive(void);

/* Append a timestamped line to the currently-open
   Error_Log_Pump_Tsueri_<date>.txt. Safe to call from any ThreadX
   thread; FileX serializes media access internally. No-op while the
   error log isn't open yet (e.g. very early boot before fx_thread has
   mounted the SD). Used by the BLE thread to record init OK/FAIL. */
void ErrorLog_Write(const char *msg);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
#ifdef __cplusplus
}
#endif
#endif /* __APP_FILEX_H__ */
