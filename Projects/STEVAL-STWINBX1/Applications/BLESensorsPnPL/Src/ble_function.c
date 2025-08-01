/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   ble_function.c
  * @author System Research & Applications Team - Agrate/Catania Lab.
  * @brief  Implementation of API called from BLE Manager
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
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include "stbox1_config.h"
#include "ble_manager.h"
#include "ble_function.h"
#include "ota.h"
#include "steval_stwinbx1.h"
#include "app_blesensorspnpl.h"

#include "STWIN.box_env_sensors.h"
#include "PnPLCompManager.h"
#include "i_control.h"
#include "i_control_vtbl.h"
#include "configuration_pn_p_l.h"
#include "control_pn_p_l.h"
#include "environmental_pn_p_l.h"
#include "inertial_pn_p_l.h"
#include "device_information_pn_p_l.h"

/* Exported Variables --------------------------------------------------------*/
volatile uint8_t  connected   = FALSE;
/*  Enable RebootBoard board */
volatile uint32_t RebootBoard = 0;
/*  Enable Swap Memory Banks */
volatile uint32_t SwapBanks   = 0;
uint32_t ConnectionBleStatus  = 0;
uint32_t SizeOfUpdateBlueFW   = 0;

int32_t CurrentEnvUpdateEnumValue = 1; /* 1Hz */
int32_t CurrentInerUpdateEnumValue = 10; /* 10Hz */

uint8_t TimerEnvIsRunning = 0;
uint8_t TimerInerIsRunning = 0;

uint32_t JSON_len_command_wTP = 0;
uint8_t *JSON_string_command_wTP = NULL;

/* Private variables ---------------------------------------------------------*/
static uint32_t NeedToRebootBoard = 0;
static uint32_t NeedToSwapBanks = 0;

static volatile int32_t PoolAvailable = 1;

/* Private functions ---------------------------------------------------------*/
uint32_t DebugConsoleCommandParsing(uint8_t *att_data, uint8_t data_length);

/**
  * @brief  Set Board Name.
  * @param  None
  * @retval None
  */
void set_board_name(void)
{
  sprintf(ble_stack_value.board_name, "%s", STBOX1_FW_PACKAGENAME);
}

/**
  * @brief  Set Custom Avertise Data.
  * @param  uint8_t *manuf_data: Avertise Data
  * @retval None
  */
void ble_set_custom_advertise_data(uint8_t *manuf_data)
{
  manuf_data[BLE_MANAGER_CUSTOM_FIELD1] = STBOX1_BLUEST_SDK_FW_ID;
  /* MCU memory bank running */
  manuf_data[BLE_MANAGER_CUSTOM_FIELD2] = CurrentActiveBank;
  manuf_data[BLE_MANAGER_CUSTOM_FIELD3] = 0x00; /* Not Used */
  manuf_data[BLE_MANAGER_CUSTOM_FIELD4] = 0x01;
}

/**
  * @brief  This function makes the parsing of the Debug Console
  * @param  uint8_t *att_data attribute data
  * @param  uint8_t data_length length of the data
  * @retval uint32_t SendBackData true/false
  */
uint32_t debug_console_parsing(uint8_t *att_data, uint8_t data_length)
{
  /* By default Answer with the same message received */
  uint32_t SendBackData = 1;

  if (SizeOfUpdateBlueFW != 0)
  {
    /* Firmware update */
    int8_t RetValue = UpdateFWBlueMS(&SizeOfUpdateBlueFW, att_data, data_length, 1);
    if (RetValue != 0)
    {
      term_update(((uint8_t *)&RetValue), 1);
      if (RetValue == 1)
      {
        /* if OTA checked */
        STBOX1_PRINTF("%s will restart after the disconnection\r\n", STBOX1_PACKAGENAME);
        HAL_Delay(1000);
        NeedToSwapBanks = 1;
      }
    }
    SendBackData = 0;
  }
  else
  {
    /* Received one write from Client on Terminal characteristc */
    SendBackData = DebugConsoleCommandParsing(att_data, data_length);
  }

  return SendBackData;
}

/**
  * @brief  This function makes the parsing of the Debug Console Commands
  * @param  uint8_t *att_data attribute data
  * @param  uint8_t data_length length of the data
  * @retval uint32_t SendBackData true/false
  */
uint32_t DebugConsoleCommandParsing(uint8_t *att_data, uint8_t data_length)
{
  uint32_t SendBackData = 1;

  /* Help Command */
  if (!strncmp("help", (char *)(att_data), 4))
  {
    /* Print Legend */
    SendBackData = 0;

    bytes_to_write = sprintf((char *)buffer_to_write,
                             "versionFw\n"
                             "info\n"
                             "uid\n");
    term_update(buffer_to_write, bytes_to_write);

  }
  else if (!strncmp("versionFw", (char *)(att_data), 9))
  {
    bytes_to_write = sprintf((char *)buffer_to_write, "%s_%s_%c.%c.%c\r\n",
                             "U585",
                             STBOX1_PACKAGENAME,
                             STBOX1_VERSION_MAJOR,
                             STBOX1_VERSION_MINOR,
                             STBOX1_VERSION_PATCH);
    term_update(buffer_to_write, bytes_to_write);
    SendBackData = 0;
  }
  else if (!strncmp("info", (char *)(att_data), 4))
  {
    SendBackData = 0;

    bytes_to_write = sprintf((char *)buffer_to_write, "\r\nSTMicroelectronics %s:\n"
                             "\tVersion %c.%c.%c\n"
                             "\tSTEVAL-STWINBX1 board"
                             "\n",
                             STBOX1_PACKAGENAME,
                             STBOX1_VERSION_MAJOR, STBOX1_VERSION_MINOR, STBOX1_VERSION_PATCH);
    term_update(buffer_to_write, bytes_to_write);

    bytes_to_write = sprintf((char *)buffer_to_write, "\t(HAL %ld.%ld.%ld_%ld)\n"
                             "\tCompiled %s %s"
#if defined (__IAR_SYSTEMS_ICC__)
                             " (IAR)\r\n",
#elif defined (__ARMCC_VERSION)
                             " (KEIL)\r\n",
#elif defined (__GNUC__)
                             " (STM32CubeIDE)\r\n",
#endif /* IDE */
                             HAL_GetHalVersion() >> 24,
                             (HAL_GetHalVersion() >> 16) & 0xFF,
                             (HAL_GetHalVersion() >> 8) & 0xFF,
                             HAL_GetHalVersion()      & 0xFF,
                             __DATE__, __TIME__);
    term_update(buffer_to_write, bytes_to_write);
    bytes_to_write = sprintf((char *)buffer_to_write, "Current Bank =%ld\n", CurrentActiveBank);
    term_update(buffer_to_write, bytes_to_write);
  }
  else if (!strncmp("upgradeFw", (char *)(att_data), 9))
  {
    uint32_t uwCRCValue;
    uint8_t *PointerByte = (uint8_t *) &SizeOfUpdateBlueFW;

    PointerByte[0] = att_data[ 9];
    PointerByte[1] = att_data[10];
    PointerByte[2] = att_data[11];
    PointerByte[3] = att_data[12];

    /* Check the Maximum Possible OTA size */
    if (SizeOfUpdateBlueFW > OTA_MAX_PROG_SIZE)
    {
      STBOX1_PRINTF("OTA %s SIZE=%ld > %d Max Allowed\r\n", STBOX1_PACKAGENAME, SizeOfUpdateBlueFW, OTA_MAX_PROG_SIZE);
      /* Answer with a wrong CRC value for signaling the problem to BlueMS application */
      buffer_to_write[0] = att_data[13];
      buffer_to_write[1] = (att_data[14] != 0) ? 0 : 1; /* In order to be sure to have a wrong CRC */
      buffer_to_write[2] = att_data[15];
      buffer_to_write[3] = att_data[16];
      bytes_to_write = 4;
      term_update(buffer_to_write, bytes_to_write);
    }
    else
    {
      PointerByte = (uint8_t *) &uwCRCValue;
      PointerByte[0] = att_data[13];
      PointerByte[1] = att_data[14];
      PointerByte[2] = att_data[15];
      PointerByte[3] = att_data[16];

      STBOX1_PRINTF("OTA %s SIZE=%ld uwCRCValue=%lx\r\n", STBOX1_PACKAGENAME, SizeOfUpdateBlueFW, uwCRCValue);

      /* Reset the Flash */
      StartUpdateFWBlueMS(SizeOfUpdateBlueFW, uwCRCValue);

#if 0
      /* Reduce the connection interval */
      {
        ble_status_t ret = aci_l2cap_connection_parameter_update_req(
                             ConnectionHandle,
                             6 /* interval_min*/,
                             6 /* interval_max */,
                             0   /* slave_latency */,
                             400 /*timeout_multiplier*/);
                             /* Go to infinite loop if there is one error */
                             if (ret != BLE_STATUS_SUCCESS)
      {
        while (1)
          {
            STBOX1_PRINTF("Problem Changing the connection interval\r\n");
          }
        }
      }
#endif /* 0 */

      /* Signal that we are ready sending back the CRV value*/
      buffer_to_write[0] = PointerByte[0];
      buffer_to_write[1] = PointerByte[1];
      buffer_to_write[2] = PointerByte[2];
      buffer_to_write[3] = PointerByte[3];
      bytes_to_write = 4;
      term_update(buffer_to_write, bytes_to_write);
    }

    SendBackData = 0;
  }
  else if (!strncmp("uid", (char *)(att_data), 3))
  {
    /* Write back the STM32 UID */
    uint8_t *uid = (uint8_t *)STM32_UUID;
    uint32_t MCU_ID = STM32_MCU_ID[0] & 0xFFF;
    bytes_to_write = sprintf((char *)buffer_to_write, "%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X_%.3lX\n",
                             uid[ 3], uid[ 2], uid[ 1], uid[ 0],
                             uid[ 7], uid[ 6], uid[ 5], uid[ 4],
                             uid[11], uid[ 10], uid[9], uid[8],
                             MCU_ID);
    term_update(buffer_to_write, bytes_to_write);
    SendBackData = 0;
  }

  return SendBackData;
}

/**
  * @brief  This function is called when the peer device get disconnected.
  * @param  None
  * @retval None
  */
void disconnection_completed_function(void)
{
  connected = FALSE;

  PoolAvailable = 1;
  JSON_len_command_wTP = 0;
  JSON_string_command_wTP = NULL;

  /* Make the device connectable again */

  /* Reset for any problem during FOTA update */
  SizeOfUpdateBlueFW = 0;

  /*Stop all the timers */
  if (W2ST_CHECK_CONNECTION(W2ST_CONNECT_BAT_EVENT))
  {
    if (HAL_TIM_OC_Stop_IT(&TIM_CC_HANDLE, TIM_CHANNEL_1) != HAL_OK)
    {
      /* Stopping Error */
      STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
    }
    STBOX1_PRINTF("Stop Battery\r\n");
  }

  /*Stop all the timers */
  if (W2ST_CHECK_CONNECTION(W2ST_CONNECT_ACC_GYRO_MAG))
  {
    if (TimerInerIsRunning)
    {
      if (HAL_TIM_OC_Stop_IT(&TIM_CC_HANDLE, TIM_CHANNEL_3) != HAL_OK)
      {
        /* Stopping Error */
        STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
      }
      STBOX1_PRINTF("Stop Iner\r\n");
      TimerInerIsRunning = 0;
    }
  }

  if (W2ST_CHECK_CONNECTION(W2ST_CONNECT_ENV))
  {
    if (TimerEnvIsRunning)
    {
      if (HAL_TIM_OC_Stop_IT(&TIM_CC_HANDLE, TIM_CHANNEL_2) != HAL_OK)
      {
        /* Stopping Error */
        STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
      }
      STBOX1_PRINTF("Stop Env\r\n");
      TimerEnvIsRunning = 0;
    }
  }

  /* Reset the BLE Connection Variable */
  ConnectionBleStatus = 0;

  if (NeedToRebootBoard)
  {
    NeedToRebootBoard = 0;
    RebootBoard = 1;
  }

  if (NeedToSwapBanks)
  {
    NeedToSwapBanks = 0;
    SwapBanks = 1;
  }

}

/**
  * @brief  This function is called when there is a LE Connection Complete event.
  * @param  None
  * @retval None
  */
void connection_completed_function(uint16_t ConnectionHandle, uint8_t Address_Type, uint8_t Addr[6])
{
  connected = TRUE;
  ConnectionBleStatus = 0;

  PoolAvailable = 1;
  JSON_len_command_wTP = 0;
  JSON_string_command_wTP = NULL;

  /* Stop the TIM Base generation in interrupt mode for Led Blinking*/
  if (HAL_TIM_OC_Stop_IT(&TIM_CC_HANDLE, TIM_CHANNEL_1) != HAL_OK)
  {
    /* Stopping Error */
    STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
  }

  BSP_LED_Off(LED_GREEN);
  HAL_Delay(100);
}

/**
  * @brief  Callback Called after a MTU Exchange Event
  * @param  int32_t MaxCharLength
  * @retval none
  */
void mtu_exchange_resp_event_function(uint16_t server_rx_mtu)
{
  if (server_rx_mtu < ble_pn_p_like_get_max_char_length())
  {
    ble_pn_p_like_set_max_char_length(server_rx_mtu);
    STBOX1_PRINTF("ble_pn_p_like_set_max_char_length ->%ld\r\n", server_rx_mtu);
  }
}

/**
  * @brief  Callback Called after a aci_gatt_tx_pool_available_event Event
  * @param  none
  * @retval none
  */
void aci_gatt_tx_pool_available_event_function(void)
{
  PoolAvailable = 1;
  STBOX1_PRINTF("aci_gatt_tx_pool_available_event_function\r\n");
}

/**
  * @brief  Encapsulate
  * @param  uint8_t *data string to write
  * @param  uint32_t length length of string to write
  * @retval ble_status_t      Status
  */
ble_status_t PnPLikeEncapsulate(uint8_t *data, uint32_t length)
{
  uint32_t length_wTP;

  int32_t MaxPnPLikeUpdate = ble_pn_p_like_get_max_char_length();
  int32_t MaxPnPLikeUpdateMinus1 = MaxPnPLikeUpdate - 1;

  if ((length % MaxPnPLikeUpdateMinus1) == 0U)
  {
    length_wTP = (length / MaxPnPLikeUpdateMinus1) + length;
  }
  else
  {
    length_wTP = (length / MaxPnPLikeUpdateMinus1) + 1U + length;
  }

  if (JSON_string_command_wTP != NULL)
  {
    STBOX1_PRINTF("BIG PROBLEM!!\r\tNot good at all\r\n");
  }
  JSON_string_command_wTP = malloc(sizeof(uint8_t) * length_wTP);

  if (JSON_string_command_wTP == NULL)
  {
    STBOX1_PRINTF("Error: Mem calloc error [%lu]: %d@%s\r\n", length, __LINE__, __FILE__);
    return BLE_STATUS_ERROR;
  }
  else
  {
    JSON_len_command_wTP = ble_command_tp_encapsulate(JSON_string_command_wTP, data, length, MaxPnPLikeUpdate);
    return BLE_STATUS_SUCCESS;
  }
}

/**
  * @brief  Send a chunk of data to PnPLike Feature
  * @param  None
  * @retval None
  */
void PnPLikeSendChunckData(void)
{
  static uint32_t j = 0;
  int32_t MaxPnPLikeUpdate = ble_pn_p_like_get_max_char_length();
  if (JSON_string_command_wTP != NULL)
  {
    uint32_t len;
    len = MIN(MaxPnPLikeUpdate, (JSON_len_command_wTP - j));
    if (ble_pn_p_like_update(JSON_string_command_wTP + j, (uint8_t)len) != (ble_status_t)BLE_STATUS_SUCCESS)
    {
      PoolAvailable = 0;
    }
    else
    {
      /* Move to next package */
      j += len;
    }

    if (j == JSON_len_command_wTP)
    {
      j = 0;
      free(JSON_string_command_wTP);
      JSON_string_command_wTP = NULL;
    }
  }
}

/**************************************************************
  * Callback functions to manage the notify/read/write events *
  *************************************************************/

/**
  * @brief  Callback Function for Un/Subscription Battery Feature
  * @param  ble_notify_event_t Event Sub/Unsub
  * @retval None
  */
void notify_event_battery(ble_notify_event_t Event)
{
  if (Event == BLE_NOTIFY_SUB)
  {
    uint32_t uhCapture = __HAL_TIM_GET_COUNTER(&TIM_CC_HANDLE);

    W2ST_ON_CONNECTION(W2ST_CONNECT_BAT_EVENT);

    /* Start the TIM Base generation in interrupt mode */
    if (HAL_TIM_OC_Start_IT(&TIM_CC_HANDLE, TIM_CHANNEL_1) != HAL_OK)
    {
      /* Starting Error */
      STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
    }

    /* Set the Capture Compare Register value */
    __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_1, (uhCapture + STBOX1_UPDATE_BATTERY));
    STBOX1_PRINTF("Start Battery\r\n");
  }
  else if (Event == BLE_NOTIFY_UNSUB)
  {
    W2ST_OFF_CONNECTION(W2ST_CONNECT_BAT_EVENT);

    /* Stop the TIM Base generation in interrupt mode */
    if (HAL_TIM_OC_Stop_IT(&TIM_CC_HANDLE, TIM_CHANNEL_1) != HAL_OK)
    {
      /* Stopping Error */
      STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
    }

    STBOX1_PRINTF("Stop Battery\r\n");
  }
}

/**
  * @brief  Callback Function for Un/Subscription Environmental Feature
  * @param  ble_notify_event_t Event Sub/Unsub
  * @retval None
  */
void notify_event_env(ble_notify_event_t Event)
{
  if (Event == BLE_NOTIFY_SUB)
  {

    if (TimerEnvIsRunning == 0)
    {
      uint32_t uhCapture = __HAL_TIM_GET_COUNTER(&TIM_CC_HANDLE);
      W2ST_ON_CONNECTION(W2ST_CONNECT_ENV);

      /* Start the TIM Base generation in interrupt mode */
      if (HAL_TIM_OC_Start_IT(&TIM_CC_HANDLE, TIM_CHANNEL_2) != HAL_OK)
      {
        /* Starting Error */
        STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
      }

      /* Set the Capture Compare Register value */
      switch (CurrentEnvUpdateEnumValue)
      {
        case 1:
          __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_2, (uhCapture + 10000));
          break;
        case 10:
          __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_2, (uhCapture + 1000));
          break;
        case 20:
          __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_2, (uhCapture + 500));
          break;
      }

      STBOX1_PRINTF("Start Env@%ldHz\r\n", CurrentEnvUpdateEnumValue);
      TimerEnvIsRunning = 1;
    }
    else
    {
      STBOX1_PRINTF("Env Already Started\r\n");
    }
  }
  else if (Event == BLE_NOTIFY_UNSUB)
  {
    W2ST_OFF_CONNECTION(W2ST_CONNECT_ENV);
    if (TimerEnvIsRunning)
    {
      /* Stop the TIM Base generation in interrupt mode */
      if (HAL_TIM_OC_Stop_IT(&TIM_CC_HANDLE, TIM_CHANNEL_2) != HAL_OK)
      {
        /* Stopping Error */
        STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
      }
      STBOX1_PRINTF("Stop Env\r\n");
      TimerEnvIsRunning = 0;
    }
    else
    {
      STBOX1_PRINTF("Env Already Stopped\r\n");
    }
  }
}

/**
  * @brief  Callback Function for Un/Subscription Inertial Feature
  * @param  ble_notify_event_t Event Sub/Unsub
  * @retval None
  */
void notify_event_inertial(ble_notify_event_t Event)
{
  if (Event == BLE_NOTIFY_SUB)
  {
    W2ST_ON_CONNECTION(W2ST_CONNECT_ACC_GYRO_MAG);
    if (TimerInerIsRunning == 0)
    {
      uint32_t uhCapture = __HAL_TIM_GET_COUNTER(&TIM_CC_HANDLE);

      /* Start the TIM Base generation in interrupt mode */
      if (HAL_TIM_OC_Start_IT(&TIM_CC_HANDLE, TIM_CHANNEL_3) != HAL_OK)
      {
        /* Starting Error */
        STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
      }

      /* Set the Capture Compare Register value */
      switch (CurrentInerUpdateEnumValue)
      {
        case 10:
          __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_3, (uhCapture + 1000));
          break;
        case 20:
          __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_3, (uhCapture + 500));
          break;
        case 30:
          __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_3, (uhCapture + 333));
          break;
      }

      STBOX1_PRINTF("Start Iner@%ldHz\r\n", CurrentInerUpdateEnumValue);
      TimerInerIsRunning = 1;
    }
    else
    {
      STBOX1_PRINTF("Iner Already Started\r\n");
    }
  }
  else if (Event == BLE_NOTIFY_UNSUB)
  {
    W2ST_OFF_CONNECTION(W2ST_CONNECT_ACC_GYRO_MAG);

    if (TimerInerIsRunning)
    {
      /* Stop the TIM Base generation in interrupt mode */
      if (HAL_TIM_OC_Stop_IT(&TIM_CC_HANDLE, TIM_CHANNEL_3) != HAL_OK)
      {
        /* Stopping Error */
        STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
      }
      STBOX1_PRINTF("Stop Iner\r\n");
      TimerInerIsRunning = 0;
    }
    else
    {
      STBOX1_PRINTF("Iner Already Stopped\r\n");
    }
  }
}

/**
  * @brief  Callback Function for Un/Subscription PnPLike Feature
  * @param  ble_notify_event_t Event Sub/Unsub
  * @retval None
  */
void notify_event_pn_p_like(ble_notify_event_t Event)
{
  if (Event == BLE_NOTIFY_SUB)
  {
    W2ST_ON_CONNECTION(W2ST_CONNECT_PNPLIKE);
    STBOX1_PRINTF("PnPLike Subscribe\r\n");
  }
  else if (Event == BLE_NOTIFY_UNSUB)
  {
    W2ST_OFF_CONNECTION(W2ST_CONNECT_PNPLIKE);
    STBOX1_PRINTF("PnPLike Unsubscribe\r\n");
  }
}

void write_request_pn_p_like_function(uint8_t *received_msg, uint32_t msg_length)
{
  PnPLCommand_t PnPLCommand;
  STBOX1_PRINTF("PnPMessage Received\r\n");
  STBOX1_PRINTF("\t<%.*s>\r\n", msg_length, received_msg);

  PnPLParseCommand((char *)received_msg, &PnPLCommand);

  if (PnPLCommand.comm_type == PNPL_CMD_GET)
  {
    char *SerializedJSON;
    uint32_t size;

    PnPLSerializeResponse(&PnPLCommand, &SerializedJSON, &size, 0);

    STBOX1_PRINTF("--> <%.*s>\r\n", size, SerializedJSON);

    PnPLikeEncapsulate((uint8_t *) SerializedJSON, size);
    free(SerializedJSON);
  }
}

/**
  * @brief  This function is called when there is a Bluetooth Read request.
  * @param  int32_t *Press Pressure Value
  * @param  uint16_t *Hum Humidity Value
  * @param  int16_t *Temp1 Temperature Number 1
  * @param  int16_t *Temp2 Temperature Number 2
  * @retval None
  */
void read_request_env_function(int32_t *Press, uint16_t *Hum, int16_t *Temp1, int16_t *Temp2)
{
  float Temperature1, Pressure;
  BSP_ENV_SENSOR_GetValue(STTS22H_0, ENV_TEMPERATURE, &Temperature1);
  BSP_ENV_SENSOR_GetValue(ILPS22QS_0, ENV_PRESSURE, &Pressure);

  *Press = (int32_t)(Pressure * 100);
  *Temp1 = (int16_t)(Temperature1 * 10);
}

/**
  * @brief  Enable Disable the jump to second flash bank and reboot board
  * @param  None
  * @retval None
  */
void EnableDisableDualBoot(void)
{
  FLASH_OBProgramInitTypeDef    OBInit;
  /* Set BFB2 bit to enable boot from Flash Bank2 */
  /* Allow Access to Flash control registers and user Flash */
  HAL_FLASH_Unlock();

  /* Allow Access to option bytes sector */
  HAL_FLASH_OB_Unlock();

  /* Get the Dual boot configuration status */
  HAL_FLASHEx_OBGetConfig(&OBInit);

  /* Enable/Disable dual boot feature */
  OBInit.OptionType = OPTIONBYTE_USER;
  OBInit.USERType   = OB_USER_SWAP_BANK;

  if (((OBInit.USERConfig) & (FLASH_OPTR_SWAP_BANK)) == FLASH_OPTR_SWAP_BANK)
  {
    OBInit.USERConfig &= ~FLASH_OPTR_SWAP_BANK;
    STBOX1_PRINTF("->Disable DualBoot\r\n");
  }
  else
  {
    OBInit.USERConfig = FLASH_OPTR_SWAP_BANK;
    STBOX1_PRINTF("->Enable DualBoot\r\n");
  }

  if (HAL_FLASHEx_OBProgram(&OBInit) != HAL_OK)
  {
    /*
    Error occurred while setting option bytes configuration.
    User can add here some code to deal with this error.
    To know the code error, user can call function 'HAL_FLASH_GetError()'
    */
    STBOX1_Error_Handler(STBOX1_ERROR_FLASH, __FILE__, __LINE__);
  }

  /* Start the Option Bytes programming process */
  if (HAL_FLASH_OB_Launch() != HAL_OK)
  {
    /*
    Error occurred while reloading option bytes configuration.
    User can add here some code to deal with this error.
    To know the code error, user can call function 'HAL_FLASH_GetError()'
    */
    STBOX1_Error_Handler(STBOX1_ERROR_FLASH, __FILE__, __LINE__);
  }
  HAL_FLASH_OB_Lock();
  HAL_FLASH_Lock();
}

/************************************************************************************
  * Callback functions to manage the extended configuration characteristic commands *
  ***********************************************************************************/

/**
  * @brief  Callback Function for answering to the UID command
  * @param  uint8_t **UID STM32 UID Return value
  * @retval None
  */
void ext_ext_config_uid_command_callback(uint8_t **UID)
{
  *UID = (uint8_t *)STM32_UUID;
}

/**
  * @brief  Callback Function for answering to VersionFw command
  * @param  uint8_t *Answer Return String
  * @retval None
  */
void ext_config_version_fw_command_callback(uint8_t *Answer)
{
  sprintf((char *)Answer, "%s_%s_%c.%c.%c\r\n",
          "U585",
          STBOX1_PACKAGENAME,
          STBOX1_VERSION_MAJOR,
          STBOX1_VERSION_MINOR,
          STBOX1_VERSION_PATCH);
}

/**
  * @brief  Callback Function for answering to Info command
  * @param  uint8_t *Answer response to command
  * @retval None
  */
void ext_config_info_command_callback(uint8_t *Answer)
{
  sprintf((char *)Answer, "\r\nSTMicroelectronics %s:\n"
          "\tVersion %c.%c.%c\n"
          "\tSTEVAL-STWINBX1 board"
          "\n\t(HAL %ld.%ld.%ld_%ld)\n"
          "\tCompiled %s %s"
#if defined (__IAR_SYSTEMS_ICC__)
          " (IAR)\n"
#elif defined (__ARMCC_VERSION)
          " (KEIL)\n"
#elif defined (__GNUC__)
          " (STM32CubeIDE)\n"
#endif /* IDE */
          "\tCurrent Bank =%ld\n",
          STBOX1_PACKAGENAME,
          STBOX1_VERSION_MAJOR, STBOX1_VERSION_MINOR, STBOX1_VERSION_PATCH,
          HAL_GetHalVersion() >> 24,
          (HAL_GetHalVersion() >> 16) & 0xFF,
          (HAL_GetHalVersion() >> 8) & 0xFF,
          HAL_GetHalVersion()      & 0xFF,
          __DATE__, __TIME__,
          CurrentActiveBank);
}

/**
  * @brief  Callback Function for answering to Help command
  * @param  uint8_t *Answer Return String
  * @retval None
  */
void ext_config_help_command_callback(uint8_t *Answer)
{
  sprintf((char *)Answer, "Help Message.....");
}

/**
  * @brief  Callback Function for answering to SetName command
  * @param  uint8_t *NewName New Name
  * @retval None
  */
void ext_config_set_name_command_callback(uint8_t *NewName)
{
  STBOX1_PRINTF("Received a new Board's Name=%s\r\n", NewName);
  /* Update the Board's name in flash */
  UpdateCurrFlashBankFwIdBoardName(STBOX1_BLUEST_SDK_FW_ID, NewName);

  /* Update the Name for BLE Advertise */
  sprintf(ble_stack_value.board_name, "%s", NewName);
}

/**
  * @brief  Callback Function for answering to ReadBanksFwId command
  * @param  uint8_t *CurBank Number Current Bank
  * @param  uint16_t *FwId1 Bank1 Firmware Id
  * @param  uint16_t *FwId2 Bank2 Firmware Id
  * @retval None
  */
void ext_config_read_banks_fw_id_command_callback(uint8_t *CurBank, uint16_t *FwId1, uint16_t *FwId2)
{
  ReadFlashBanksFwId(FwId1, FwId2);
  *CurBank = CurrentActiveBank;
}

/**
  * @brief  Callback Function for answering to BanksSwap command
  * @param  None
  * @retval None
  */
void ext_config_banks_swap_command_callback(void)
{
  uint16_t FwId1;
  uint16_t FwId2;

  /* Check memory banks for firmwares present identification */
  /* Swapping Bank is possible if on to both memory banks there is a firmware */
  ReadFlashBanksFwId(&FwId1, &FwId2);
  if (FwId2 != OTA_OTA_FW_ID_NOT_VALID)
  {
    STBOX1_PRINTF("Swapping to Bank%d\n", (CurrentActiveBank == 1) ? 0 : 1);
    STBOX1_PRINTF("%s will restart after the disconnection\r\n", STBOX1_PACKAGENAME);
    NeedToSwapBanks = 1;
  }
  else
  {
    STBOX1_PRINTF("Not Valid fw on Bank%d\n\tCommand Rejected\n", (CurrentActiveBank == 1) ? 0 : 1);
    STBOX1_PRINTF("\tLoad a Firmware on Bank%d\n", (CurrentActiveBank == 1) ? 0 : 1);
  }
}

