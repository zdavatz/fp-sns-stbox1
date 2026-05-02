/**
  ******************************************************************************
  * @file    RTE_Components.h
  * @brief   Compile-time feature flags consumed by the X-CUBE-BLEMGR
  *          (STM32_BLE_Manager) middleware.
  *
  * Trimmed for SDDataLogFileX: only the BLE-stack basics are enabled.
  * The sensor-streaming services that BLEDualProgram pulls in
  * (BLE_MANAGER_ENVIRONMENTAL / INERTIAL / SENSOR_FUSION, plus the NFC
  * lib_NDEF stack) are deliberately omitted — we only need a single
  * custom GATT service for file sync, registered from
  * ble_implementation.c::ble_init_custom_service().
  ******************************************************************************
  */

#ifndef __RTE_COMPONENTS_H__
#define __RTE_COMPONENTS_H__

/* X-CUBE-BLEMGR core */
#define HCI_TL
#define HCI_TL_INTERFACE
#define USE_PARSON
#define STM32WB07_06

#endif /* __RTE_COMPONENTS_H__ */
