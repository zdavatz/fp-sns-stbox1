/**
  ******************************************************************************
  * @file    ble_function.c
  * @brief   Minimal BLE-manager glue for SDDataLogFileX. Trimmed from
  *          BLEDualProgram — no FOTA, no sensor notify hooks, no debug
  *          console parsing (ENABLE_CONSOLE = 0). Just the mandatory
  *          globals + connection callbacks + the ext-config callbacks
  *          whose flags we left enabled in ble_implementation.h.
  ******************************************************************************
  */

#include <stdio.h>
#include <string.h>
#include "ble_manager.h"
#include "ble_implementation.h"
#include "ble_filesync.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  1
#endif

/* Mandatory globals referenced by ble_manager.c */
volatile uint8_t  connected            = FALSE;
volatile uint8_t  paired               = FALSE;
volatile uint32_t RebootBoard          = 0;
volatile uint32_t SwapBanks            = 0;
volatile uint32_t NeedToClearSecureDB  = 0;
uint32_t          ConnectionBleStatus  = 0;

/* BlueST-SDK firmware-id slot for the FileSync app — picked away from the
   BLEDualProgram value (0x03) so a host can tell the apps apart in scans. */
#define STBOXSYNC_BLUEST_SDK_FW_ID  0x10

void set_board_name(void)
{
  sprintf(ble_stack_value.board_name, "%s", BLE_FW_PACKAGENAME);
}

void ble_set_custom_advertise_data(uint8_t *manuf_data)
{
  ble_stack_value.board_id = BLE_MANAGER_SENSOR_TILE_BOX_PRO_C_PLATFORM;
  manuf_data[BLE_MANAGER_CUSTOM_FIELD1 - 1] = ble_stack_value.board_id;
  manuf_data[BLE_MANAGER_CUSTOM_FIELD1]     = STBOXSYNC_BLUEST_SDK_FW_ID;
  manuf_data[BLE_MANAGER_CUSTOM_FIELD2]     = 0x01; /* bank — single-image build, fixed */
  manuf_data[BLE_MANAGER_CUSTOM_FIELD3]     = 0x00;
  manuf_data[BLE_MANAGER_CUSTOM_FIELD4]     = 0x00;
}

void connection_completed_function(uint16_t ConnectionHandle, uint8_t Address_Type, uint8_t Addr[6])
{
  (void)ConnectionHandle;

  connected = TRUE;
  ConnectionBleStatus = 0;

  /* Honour previously-bonded peers so the user only enters the PIN once. */
  if (aci_gap_is_device_bonded(Address_Type, Addr) == BLE_STATUS_SUCCESS)
  {
    paired = TRUE;
  }
}

void disconnection_completed_function(void)
{
  connected = FALSE;
  paired    = FALSE;
  ConnectionBleStatus = 0;
  /* Reset the FileSync state machine so a mid-LIST or mid-READ drop
     doesn't poison the next reconnect — without this, the next LIST
     opcode is silently dropped because state != ST_IDLE, and the user
     sees an empty file list after Refresh. */
  BleFileSync_Reset();
}

void pairing_completed_function(uint8_t PairingStatus)
{
  paired = (PairingStatus == 0x00) ? TRUE : FALSE;
}

/* ---------------- Ext-config callbacks (only the ones flagged on) ---------- */

void ext_ext_config_uid_command_callback(uint8_t **UID)
{
  *UID = (uint8_t *)BLE_STM32_UUID;
}

void ext_config_version_fw_command_callback(uint8_t *Answer)
{
  sprintf((char *)Answer, "%s_%s_%c.%c.%c\r\n",
          BLE_STM32_MICRO,
          BLE_FW_PACKAGENAME,
          BLE_VERSION_FW_MAJOR,
          BLE_VERSION_FW_MINOR,
          BLE_VERSION_FW_PATCH);
}

void ext_config_info_command_callback(uint8_t *Answer)
{
  sprintf((char *)Answer,
          "\r\nywesee %s:\n"
          "\tVersion %c.%c.%c\n"
          "\tSTEVAL-MKBOXPRO Rev C\n"
          "\tCompiled %s %s\n",
          BLE_FW_PACKAGENAME,
          BLE_VERSION_FW_MAJOR, BLE_VERSION_FW_MINOR, BLE_VERSION_FW_PATCH,
          __DATE__, __TIME__);
}

void ext_config_help_command_callback(uint8_t *Answer)
{
  sprintf((char *)Answer,
          "STBoxSync — file sync over BLE.\n"
          "Use the MovementLogger GUI to scan, connect, and download CSV/log files.\n");
}

void ext_config_set_name_command_callback(uint8_t *NewName)
{
  /* No persistence layer here — just update the live advertised name. */
  sprintf(ble_stack_value.board_name, "%s", (char *)NewName);
}

void ext_config_clear_db_command_callback(void)
{
  NeedToClearSecureDB = 1;
}
