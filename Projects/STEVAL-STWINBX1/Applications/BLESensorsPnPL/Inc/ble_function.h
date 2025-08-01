/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ble_function.h
  * @author  System Research & Applications Team - Agrate/Catania Lab.
  * @brief   ble_function header File
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
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _BLE_FUNCTION_H_
#define _BLE_FUNCTION_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ble_battery.h"
#include "ble_environmental.h"
#include "ble_sensor_fusion.h"
#include "ble_inertial.h"

/* Exported Defines --------------------------------------------------------*/
/* Firmware Package Name */
#define STBOX1_FW_PACKAGENAME      "BLEPnP"

/* Environmental Data */
#define W2ST_CONNECT_ENV           (1   )

/* Acceleration/Gyroscope/Magneto */
#define W2ST_CONNECT_ACC_GYRO_MAG  (1<<1)

/* Standard Terminal */
#define W2ST_CONNECT_STD_TERM      (1<<2)

/* Standard Error */
#define W2ST_CONNECT_STD_ERR       (1<<3)

/* Battery Feature */
#define W2ST_CONNECT_BAT_EVENT     (1<<4)

/* PnPLike Feature */
#define W2ST_CONNECT_PNPLIKE       (1<<5)

/* Exported Variables ------------------------------------------------------- */
extern uint32_t ConnectionBleStatus;
extern volatile uint8_t  connected;
extern volatile uint32_t RebootBoard;
extern volatile uint32_t SwapBanks;

extern int32_t CurrentEnvUpdateEnumValue;
extern int32_t CurrentInerUpdateEnumValue;
extern uint32_t JSON_len_command_wTP;
extern uint8_t *JSON_string_command_wTP;

/* Exported macro ------------------------------------------------------------*/
#define W2ST_CHECK_CONNECTION(BleChar) ((ConnectionBleStatus&(BleChar)) ? 1 : 0)
#define W2ST_ON_CONNECTION(BleChar)    (ConnectionBleStatus|=(BleChar))
#define W2ST_OFF_CONNECTION(BleChar)   (ConnectionBleStatus&=(~BleChar))

/* Exported Functions Prototypes ---------------------------------------------*/
extern void EnableDisableDualBoot(void);

extern ble_status_t PnPLikeEncapsulate(uint8_t *data, uint32_t length);
extern void PnPLikeSendChunckData(void);

/* Private Functions Prototypes ----------------------------------------------*/
void MTUExcahngeRespEvent(int32_t MaxCharLength);
void TxPoolAvailableEvent(void);

#ifdef __cplusplus
}
#endif

#endif /* _BLE_FUNCTION_H_ */
