/**
  ******************************************************************************
  * @file    ble_manager_conf.h
  * @brief   Configuration for the STM32_BLE_Manager middleware.
  *          Copied verbatim from BLEDualProgram — same chip (STM32WB07_06),
  *          same security defaults (PIN-secure pairing), same MTU sizes.
  *
  * Notable settings (don't change without testing):
  *   BLUE_CORE STM32WB07_06          — selects the right HCI binding
  *   BLE_MANAGER_USE_PARSON          — needed for ext-config JSON parsing
  *   DEFAULT_MAX_CHAR_LEN 255        — sets FileData notify buffer ceiling
  *   SECURE_CONNECTION_SUPPORT_OPTION_CODE 0x01 — required for "pair once"
  *   ADDRESS_TYPE 1                  — random BLE address (per-boot)
  ******************************************************************************
  */

#ifndef __BLE_MANAGER_CONF_H__
#define __BLE_MANAGER_CONF_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Bluetooth core selection (must match the middleware sources we link in). */
#define BLUENRG_1_2     0x00
#define BLUENRG_MS      0x01
#define BLUENRG_LP      0x02
#define BLUE_WB         0x03
#define STM32WB07_06    0x04
#define STM32WB05N      0x05
#define BLUE_CORE       STM32WB07_06

#define BLE_MANAGER_USE_PARSON
#ifndef BLE_MANAGER_USE_PARSON
#define BLE_MANAGER_NO_PARSON
#endif

#define OUT_OF_BAND_ENABLEDATA                       0x00
#define DEFAULT_MAX_CONFIG_CHAR_LEN                  20
#define ADDRESS_TYPE                                 1
#define ENABLE_HIGH_POWER_MODE                       0x01
#define POWER_AMPLIFIER_OUTPUT_LEVEL                 0x04
#define CONFIG_VALUE_LENGTH                          6
#define GAP_ROLES                                    0x01
#define CONFIG_VALUE_OFFSETS                         0x00
#define BLE_MANAGER_MAX_ALLOCABLE_CHARS              32
#define DEFAULT_MAX_STDERR_CHAR_LEN                  244
#define DEFAULT_MAX_CHAR_LEN                         255
#define MITM_PROTECTION_REQUIREMENTS                 0x01
#define IO_CAPABILITIES                              0x00
#define AUTHENTICATION_REQUIREMENTS                  0x01
#define SECURE_CONNECTION_SUPPORT_OPTION_CODE        0x01
#define SECURE_CONNECTION_KEYPRESS_NOTIFICATION      0x00
#define ADVERTISING_FILTER                           0x00

#define BLE_MANAGER_SDKV2

#define DEFAULT_MAX_STDOUT_CHAR_LEN     DEFAULT_MAX_CHAR_LEN
#define DEFAULT_MAX_EXTCONFIG_CHAR_LEN  DEFAULT_MAX_CHAR_LEN

#define ACC_BLUENRG_CONGESTION

#define BLE_MANAGER_DELAY HAL_Delay

/* BLE_INITIAL_DELAY override — used by `init_ble_manager_ble_stack()` at
   three explicit settle-points (after hci_reset, after aci_gap_init, after
   aci_hal_set_tx_power_level — 2 s each, 6 s total). Without this override
   the middleware falls back to `BLE_MANAGER_DELAY = HAL_Delay`, which is a
   SysTick busy-loop that does NOT yield to the ThreadX scheduler — that
   means 6 s of CPU monopoly by whichever thread happens to be running BLE
   bring-up. With BLE thread at priority 14 (below fx_thread = 12), this
   monopoly happens during fx_thread's blocked-on-queue idle and is
   harmless. But if BLE priority is ever raised above fx_thread (as in
   v41/v48/v49 at priority 11), the 6 s busy-wait starves fx_thread
   completely and the SD logger can't open the card. The override below
   uses `tx_thread_sleep` so the BLE thread yields the CPU during these
   settles, surviving any future priority change. ms is converted to
   ThreadX ticks (10 ms each); minimum 1 tick. */
#include "tx_api.h"
static inline void BleManager_InitialDelay(uint32_t ms)
{
  ULONG ticks = (ms + 9U) / 10U;
  if (ticks == 0U) ticks = 1U;
  tx_thread_sleep(ticks);
}
#define BLE_INITIAL_DELAY BleManager_InitialDelay

#define BLE_MALLOC_FUNCTION  malloc
#define BLE_FREE_FUNCTION    free
#define BLE_MEM_CPY          memcpy

/* BLE manager's own debug printf — compiled out because UART4 is the GPS in
   this app and there's no other free serial. Boot/sync events go to the SD
   error log instead (see ble_sync.c → ErrorLog_Write). */
#define BLE_MANAGER_PRINTF(...)

#ifdef __cplusplus
}
#endif

#endif /* __BLE_MANAGER_CONF_H__ */
