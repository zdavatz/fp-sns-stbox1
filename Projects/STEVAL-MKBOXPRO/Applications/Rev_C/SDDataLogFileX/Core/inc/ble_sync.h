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

/**
 * @brief  Run the full BLE bring-up (chip-alive probe + bluetooth_init +
 *         EXTI11 arm) synchronously from main() BEFORE MX_ThreadX_Init.
 *         Called pre-kernel so there is no fx_thread / no concurrent SDMMC
 *         activity to be corrupted by the BlueNRG-LP HCI_Reset SPI traffic
 *         on the 3.3V-modded reservebox (issue #12). After this call
 *         returns the kernel can start; ble_sync_thread sees the result in
 *         g_ble_probe_status and either enters its event loop (status=1)
 *         or parks (status=0/2). No-op when STBOX1_ENABLE_BLE_SYNC=0.
 */
void BleSync_PreKernelInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_SYNC_H */
