/**
  ******************************************************************************
  * @file    ble_sync.h
  * @brief   BLE file-sync service for SDDataLogFileX. Lets the host download
  *          SensNNN.csv / GpsNNN.csv / BatNNN.csv / Error_Log_*.log from the
  *          SD card over BLE so no card swap is needed.
  *
  * Currently a build-time-gated stub. Real GATT service + BlueNRG-LP init
  * land in follow-up commits. With STBOX1_ENABLE_BLE_SYNC=0 the call below
  * is a no-op returning TX_SUCCESS, so the logger firmware is unchanged.
  ******************************************************************************
  */

#ifndef __BLE_SYNC_H
#define __BLE_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tx_api.h"

/**
 * @brief  Allocate stacks, queues and create the BLE-sync thread from the
 *         supplied byte pool. Called from App_ThreadX_Init().
 * @param  byte_pool  ThreadX byte pool to allocate from.
 * @retval TX_SUCCESS on success (or when STBOX1_ENABLE_BLE_SYNC=0).
 */
UINT BleSync_ThreadX_Init(TX_BYTE_POOL *byte_pool);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_SYNC_H */
