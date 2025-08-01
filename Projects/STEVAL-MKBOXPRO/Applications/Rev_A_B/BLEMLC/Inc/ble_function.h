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
#include "ble_finite_state_machine.h"
#include "ble_machine_learning_core.h"
#include "ble_activity_recognition.h"
#include "ble_inertial.h"
#include "SensorTileBoxPro_conf.h"

/* Exported Defines --------------------------------------------------------*/
/* Firmware Package Name */
#define STBOX1_FW_PACKAGENAME      "BLEMCL"

/* Acceleration/Gyroscope/Magneto */
#define W2ST_CONNECT_ACC_GYRO_MAG  (1)

/* Activity Recognition */
#define W2ST_CONNECT_AR_EVENT   (1<<1)

/* Machine Learning Core */
#define W2ST_CONNECT_MLC        (1<<2)

/* Finite State Machine */
#define W2ST_CONNECT_FSM        (1<<3)

/* Exported Variables ------------------------------------------------------- */
extern uint32_t ConnectionBleStatus;
extern volatile uint8_t  connected;
extern volatile uint32_t RebootBoard;
extern volatile uint32_t SwapBanks;

/* Exported macro ------------------------------------------------------------*/
#define W2ST_CHECK_CONNECTION(BleChar) ((ConnectionBleStatus&(BleChar)) ? 1 : 0)
#define W2ST_ON_CONNECTION(BleChar)    (ConnectionBleStatus|=(BleChar))
#define W2ST_OFF_CONNECTION(BleChar)   (ConnectionBleStatus&=(~BleChar))

/* Exported Functions Prototypes ---------------------------------------------*/
extern void EnableDisableDualBoot(void);

/* Private Functions Prototypes ----------------------------------------------*/
void ReadRequestActRec(ble_ar_output_t *ActCode, ble_ar_algo_idx_t *Algorithm);
void ReadMachineLearningCore(uint8_t *mlc_out, uint8_t *mlc_status_mainpage);
void ReadFiniteStateMachine(uint8_t *fsm_out, uint8_t *fsm_status_a_mainpage, uint8_t *fsm_status_b_mainpage);

#ifdef __cplusplus
}
#endif

#endif /* _BLE_FUNCTION_H_ */
