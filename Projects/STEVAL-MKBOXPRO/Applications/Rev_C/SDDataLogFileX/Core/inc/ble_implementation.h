/**
  ******************************************************************************
  * @file    ble_implementation.h
  * @brief   Application-side BLE configuration consumed by ble_manager.c.
  *          Trimmed from the BLEDualProgram template — the
  *          ble_environmental.h / ble_inertial.h / ble_sensor_fusion.h
  *          includes are gone because we only register the file-sync
  *          custom service.
  *
  * If you flip an ENABLE_*_EXT_CONFIG flag below, also enable the
  * corresponding handler in ble_function.c. Defaults match BLEDualProgram
  * for everything we kept (secure-pairing PIN, board-name set, version
  * report) and disable everything that needed the FOTA/sensor handlers.
  ******************************************************************************
  */

#ifndef _BLE_IMPLEMENTATION_H_
#define _BLE_IMPLEMENTATION_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STM32U5xx

/* Ext-config command surface — keep the read-only ones, drop everything
   that would need FOTA/sensor hooks we don't ship. */
#define ENABLE_READ_CUSTOM_COMMAND_EXT_CONFIG       0
#define ENABLE_EXT_CONFIG                           1
#define ENABLE_CONFIG                               1
#define ENABLE_SET_DATE_EXT_CONFIG                  0
#define ENABLE_CHANGE_SECURE_PIN_EXT_CONFIG         0
#define ENABLE_SECURE_CONNECTION                    1  /* match BLEDualProgram exactly */
#define ENABLE_VERSION_FW_EXT_CONFIG                1
#define ENABLE_READ_CERTIFICATE_EXT_CONFIG          0
#define ENABLE_CLEAR_SECURE_DATA_BASE_EXT_CONFIG    1
#define ENABLE_SET_NAME_EXT_CONFIG                  1
#define ENABLE_REBOOT_ON_DFU_MODE_EXT_CONFIG        0
#define ENABLE_POWER_OFF_EXT_CONFIG                 0
#define ENABLE_HELP_EXT_CONFIG                      1
#define SECURE_PIN                                  123456
#define ENABLE_RANDOM_SECURE_PIN                    0
/* Debug-console BLE service is off — saves us a debug_console_parsing()
   stub. The host doesn't need it; FileSync runs over our own GATT service. */
#define ENABLE_CONSOLE                              0
#define ENABLE_SET_CERTIFICATE_EXT_CONFIG           0
#define ENABLE_STM32_UID_EXT_CONFIG                 1
#define ENABLE_SET_WIFI_EXT_CONFIG                  0
#define ENABLE_INFO_EXT_CONFIG                      1
#define ENABLE_SET_TIME_EXT_CONFIG                  0
#define ENABLE_READ_BANKS_FW_ID_EXT_CONFIG          0
#define USED_PLATFORM                               0x13U  /* SENSOR_TILE_BOX_PRO */
#define ENABLE_POWER_STATUS_EXT_CONFIG              0
#define ENABLE_BANKS_SWAP_EXT_CONFIG                0

#define BLE_MANAGER_USED_PLATFORM   USED_PLATFORM
#define BLE_STM32_UUID              UID_BASE
#ifdef DBGMCU_BASE
#define BLE_STM32_MCU_ID            ((uint32_t *)DBGMCU_BASE)
#else
#define BLE_STM32_MCU_ID            ((uint32_t *)0x00000000UL)
#endif
#define BLE_STM32_MICRO             "STM32U5xx"

/* Reported by ble_manager when the host queries firmware version. */
#define BLE_VERSION_FW_MAJOR        '0'
#define BLE_VERSION_FW_MINOR        '1'
#define BLE_VERSION_FW_PATCH        '0'
#define BLE_FW_PACKAGENAME          "PumpTsueri"  /* 15 chars max — board_name[16] in ble_manager */

/* Implemented in ble_implementation.c. ble_manager.c calls bluetooth_init()
   from init_ble_manager() and ble_init_custom_service() right after the
   stack is up; the others are weak overrides we leave at the defaults.
   bluetooth_init returns the ble_manager status — non-zero means the
   BlueNRG-LP didn't come up and the BLE thread should bail without
   arming the SPI EXTI IRQ. */
extern uint8_t bluetooth_init(void);
extern void init_ble_int_for_blue_nrglp(void);
extern void arm_ble_exti11(void);  /* issue #12: final EXTI11 NVIC enable, post-init */
extern void ble_init_custom_service(void);
extern void ble_set_custom_advertise_data(uint8_t *manuf_data);
extern void set_board_name(void);
extern void enable_extended_configuration_command(void);

#if (BLUE_CORE != BLUE_WB)
extern void reset_ble_manager(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _BLE_IMPLEMENTATION_H_ */
