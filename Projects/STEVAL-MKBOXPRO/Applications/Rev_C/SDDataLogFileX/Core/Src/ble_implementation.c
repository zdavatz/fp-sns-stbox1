/**
  ******************************************************************************
  * @file    ble_implementation.c
  * @brief   App-side BLE bring-up. Adapted from BLEDualProgram —
  *          ble_init_custom_service() is empty for now (the FileSync GATT
  *          characteristics land in slice 3); we just want the box to
  *          advertise as STBoxSync with PIN-secure pairing.
  ******************************************************************************
  */

#include <stdio.h>
#include "ble_manager.h"
#include "ble_implementation.h"
#include "ble_filesync.h"
#include "hci_tl_interface.h"

void bluetooth_init(void)
{
  /* GAP / security setup — values come from ble_manager_conf.h */
  ble_stack_value.config_value_offsets                    = CONFIG_VALUE_OFFSETS;
  ble_stack_value.config_value_length                     = CONFIG_VALUE_LENGTH;
  ble_stack_value.gap_roles                               = GAP_ROLES;
  ble_stack_value.io_capabilities                         = IO_CAPABILITIES;
  ble_stack_value.authentication_requirements             = AUTHENTICATION_REQUIREMENTS;
  ble_stack_value.mitm_protection_requirements            = MITM_PROTECTION_REQUIREMENTS;
  ble_stack_value.secure_connection_support_option_code   = SECURE_CONNECTION_SUPPORT_OPTION_CODE;
  ble_stack_value.secure_connection_keypress_notification = SECURE_CONNECTION_KEYPRESS_NOTIFICATION;
  ble_stack_value.own_address_type                        = ADDRESS_TYPE;

  set_board_name();

  ble_stack_value.enable_high_power_mode       = ENABLE_HIGH_POWER_MODE;
  ble_stack_value.power_amplifier_output_level = POWER_AMPLIFIER_OUTPUT_LEVEL;

  ble_stack_value.enable_config     = ENABLE_CONFIG;
  ble_stack_value.enable_console    = ENABLE_CONSOLE;
  ble_stack_value.enable_ext_config = ENABLE_EXT_CONFIG;

  ble_stack_value.enable_secure_connection  = ENABLE_SECURE_CONNECTION;
  ble_stack_value.secure_pin                = SECURE_PIN;
  ble_stack_value.enable_random_secure_pin  = ENABLE_RANDOM_SECURE_PIN;

  ble_stack_value.board_id = BLE_MANAGER_USED_PLATFORM;

  /* Secure Connection chip-side rescan (force_rescan=0); when secure off,
     the host has to invalidate its GATT cache itself. */
  ble_stack_value.force_rescan = ble_stack_value.enable_secure_connection ? 0 : 1;

  init_ble_manager();
}

void init_ble_int_for_blue_nrglp(void)
{
  HAL_EXTI_GetHandle(&H_EXTI, EXTI_LINE);
  HAL_EXTI_RegisterCallback(&H_EXTI, HAL_EXTI_COMMON_CB_ID, hci_tl_lowlevel_isr);
  HAL_NVIC_SetPriority(HCI_TL_SPI_EXTI_IRQ_N, 0, 0);
  HAL_NVIC_EnableIRQ(HCI_TL_SPI_EXTI_IRQ_N);
}

void ble_init_custom_service(void)
{
  /* Both characteristics live under the BlueST features service that
     ble_manager owns; the manager sweeps the chars-array after this
     callback returns and adds them all in one shot. */
  BleFileSync_Register();
}
