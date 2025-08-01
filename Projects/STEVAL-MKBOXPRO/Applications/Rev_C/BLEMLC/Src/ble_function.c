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
#include "steval_mkboxpro.h"
#include "app_blemlc.h"

#include "SensorTileBoxPro_motion_sensors_ex.h"
#include "uzlib.h"

/* Exported Variables --------------------------------------------------------*/
volatile uint8_t  connected   = FALSE;
/*  Enable RebootBoard board */
volatile uint32_t RebootBoard = 0;
/*  Enable Swap Memory Banks */
volatile uint32_t SwapBanks   = 0;
uint32_t ConnectionBleStatus  = 0;
uint32_t SizeOfUpdateBlueFW   = 0;

/* Private variables ---------------------------------------------------------*/
static uint32_t NeedToRebootBoard = 0;
static uint32_t NeedToSwapBanks = 0;

static int32_t malloc_count = 0;
static int32_t malloc_size = 0;

/* Private functions ---------------------------------------------------------*/
uint32_t DebugConsoleCommandParsing(uint8_t *att_data, uint8_t data_length, uint32_t *DecodingOneStream,
                                    int32_t *StreamLength, uint8_t **CompressedData);

/* Function for decompressing the MCL/FSM program */
static uint32_t GetUncompressedSize(uint8_t *compressed, uint32_t size);
static uint8_t *Decompress(uint8_t *compressed, uint32_t size, uint32_t *UnComSize);
static void FromHexToUCF(const char *In, uint32_t len, ucf_line_t *UCFProgram);

static void *counted_malloc(size_t size);
static void counted_free(void *ptr);

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
  ble_stack_value.board_id = BLE_MANAGER_SENSOR_TILE_BOX_PRO_C_PLATFORM;
  /* STM32 Board used */
  manuf_data[BLE_MANAGER_CUSTOM_FIELD1 - 1] = ble_stack_value.board_id;
  manuf_data[BLE_MANAGER_CUSTOM_FIELD1] = STBOX1C_BLUEST_SDK_FW_ID;
  /* MCU memory bank running */
  manuf_data[BLE_MANAGER_CUSTOM_FIELD2] = CurrentActiveBank;
  manuf_data[BLE_MANAGER_CUSTOM_FIELD3] = 0x00; /* Not Used */
  manuf_data[BLE_MANAGER_CUSTOM_FIELD4] = 0x00; /* Not Used */
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

  static uint32_t DecodingOneStream = 0;
  static int32_t StreamLength = -1; /* Nothing to Decode */
  static uint8_t *CompressedData = NULL;
  static int32_t PointerToCompressData = 0;
  static uint8_t *DeCompressedData = NULL;

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
    if (DecodingOneStream)
    {
      /* If we are decoding one stream */
      /* we receive packets of 20 bytes */
      /*STBOX1_PRINTF("Stream StreamLength=%d packet=%d\r\n",StreamLength,data_length);*/
      memcpy(CompressedData + PointerToCompressData, att_data, data_length);
      StreamLength -= data_length;
      PointerToCompressData += data_length;
      /* Return Message */
      term_update(att_data, data_length);
      SendBackData = 0;
    }
    else
    {
      /* Received one write from Client on Terminal characteristc */
      SendBackData = DebugConsoleCommandParsing(att_data,
                                                data_length,
                                                &DecodingOneStream,
                                                &StreamLength,
                                                &CompressedData);
    }
  }

  /* Decode the Stream full received */
  if (StreamLength == 0)
  {
    /* Decode the stream */
    uint32_t UnComSize;
    STBOX1_PRINTF("End of Stream\r\n");
    /* Set Nothing to Receive */
    StreamLength = -1;
    DecodingOneStream = 0;
    /* Decompressed the Data */
    STBOX1_PRINTF("--- Json Start Decompression ---\r\n");
    DeCompressedData = Decompress(CompressedData, PointerToCompressData, &UnComSize);
    STBOX1_PRINTF("--- Json End Decompression ---\r\n");
    /* Free Memory For Compressed Data*/
    free(CompressedData);
    CompressedData = NULL;
    PointerToCompressData = 0;

    if (DeCompressedData == NULL)
    {
      /* Return Message */
      bytes_to_write = sprintf((char *)buffer_to_write, "Flow_parse_ko");
      term_update(buffer_to_write, bytes_to_write);
    }
    else
    {
      JSON_Value *root_value = NULL;
      STBOX1_PRINTF("--- Json Start Parsing ---\r\n");
#ifdef STBOX1_ENABLE_PRINTF
      json_set_allocation_functions(counted_malloc, counted_free);
#endif /* STBOX1_ENABLE_PRINTF */
      /* Parse the Decompressed Data */
      root_value = json_parse_string((char *)DeCompressedData);
      /* Free Memory For Decompressed Data*/
      free(DeCompressedData);
      DeCompressedData = NULL;
      if (json_value_get_type(root_value) == JSONArray)
      {
        STBOX1_PRINTF("root_value==ARRAY\r\n");
      }
      JSON_Array *flows;
      int32_t NumFlows;
      flows = json_value_get_array(root_value);
      STBOX1_PRINTF("Num flows=%d\r\n", json_array_get_count(flows));
      for (NumFlows = 0; NumFlows < json_array_get_count(flows); NumFlows++)
      {
        JSON_Object *flow;
        int32_t NumElementFlow;
        flow = json_array_get_object(flows, NumFlows);
        STBOX1_PRINTF("\tNumElementFlow=%d\r\n", json_object_get_count(flow));
        for (NumElementFlow = 0; NumElementFlow < json_object_get_count(flow); NumElementFlow++)
        {
          STBOX1_PRINTF("\tElementName=%s\r\n", json_object_get_name(flow, NumElementFlow));
          if (!strncmp("sensors", json_object_get_name(flow, NumElementFlow), 7))
          {
            JSON_Array  *SensorsArray;
            int32_t NumSensors;
            SensorsArray = json_object_get_array(flow, "sensors");
            STBOX1_PRINTF("\t\tNumSensors=%d\r\n", json_array_get_count(SensorsArray));
            for (NumSensors = 0; NumSensors < json_array_get_count(SensorsArray); NumSensors++)
            {
              JSON_Object *Sensor;
              Sensor = json_array_get_object(SensorsArray, NumSensors);
              if (!strncmp("S12", json_object_get_string(Sensor, "id"), 3))
              {
                /* Check if we have one program for MLC */
                const char *regConfig;
                const char *mlcLabels;
                STBOX1_PRINTF("\t\t\tMLC Sensor ID found\r\n");
                regConfig = json_object_dotget_string(Sensor, "configuration.regConfig");
                if (regConfig != NULL)
                {
                  uint32_t Length = strlen(regConfig);
                  STBOX1_PRINTF("\t\t\tMLC Reg Config [%ld] found\r\n", Length);
                  /* Allocate the Memory for the CustomUCF Program for MLC */
                  /* Length should be a multiple of 4 */
                  if (Length & 0x3)
                  {
                    /* Error */
                    STBOX1_PRINTF("Error Reg Config length not multiple of 4\r\n");
                  }
                  else
                  {
                    if (MLCCustomUCFFile != NULL)
                    {
                      /* if there is already one MLCCustomUCFFile...Release the Memory before */
                      free(MLCCustomUCFFile);
                      MLCCustomUCFFile = NULL;
                      MLCCustomUCFFileLength = 0;
                    }
                    MLCCustomUCFFile = (ucf_line_t *)calloc(Length >> 2, sizeof(ucf_line_t));
                    if (MLCCustomUCFFile == NULL)
                    {
                      STBOX1_PRINTF("Error in memory allocation MLCCustomUCFFile\r\n");
                    }
                    else
                    {
                      MLCCustomUCFFileLength = Length >> 2;
                      FromHexToUCF(regConfig, Length, MLCCustomUCFFile);
                    }
                  }
                }
                mlcLabels = json_object_dotget_string(Sensor, "configuration.mlcLabels");
                if (mlcLabels != NULL)
                {
                  uint32_t Length = strlen(mlcLabels);
                  STBOX1_PRINTF("\t\t\tMLC Labels [%ld] found\r\n", Length);
                  /* Allocate the Memory for the Custom Labels for MLC */
                  extern char *MLCCustomLabels;
                  if (MLCCustomLabels != NULL)
                  {
                    /* if there is already one MLCCustomLabels...Release the Memory before */
                    free(MLCCustomLabels);
                    MLCCustomLabels = NULL;
                  }
                  MLCCustomLabels = (char *)calloc(Length + 1, sizeof(char));
                  if (MLCCustomLabels == NULL)
                  {
                    STBOX1_PRINTF("Error in memory allocation MLCCustomLabels\r\n");
                  }
                  else
                  {
                    MLCCustomLabelsLength = Length + 1;
                    memcpy(MLCCustomLabels, mlcLabels, Length);
                    /* Put Termination */
                    MLCCustomLabels[Length] = '\n';
                  }
                }
              }
              else if (!strncmp("S13", json_object_get_string(Sensor, "id"), 3))
              {
                /* Check if we have one program for FSM */
                const char *regConfig;
                const char *fsmLabels;
                STBOX1_PRINTF("\t\t\tFSM Sensor ID found\r\n");
                regConfig = json_object_dotget_string(Sensor, "configuration.regConfig");
                if (regConfig != NULL)
                {
                  uint32_t Length = strlen(regConfig);
                  STBOX1_PRINTF("\t\t\tFSM Reg Config [%ld] found\r\n", Length);
                  /* Allocate the Memory for the CustomUCF Program for FSM */
                  /* Length should be a multiple of 4 */
                  if (Length & 0x3)
                  {
                    /* Error */
                    STBOX1_PRINTF("Error Reg Config length not multiple of 4\r\n");
                  }
                  else
                  {
                    if (FSMCustomUCFFile != NULL)
                    {
                      /* if there is already one FSMCustomUCFFile...Release the Memory before */
                      free(FSMCustomUCFFile);
                      FSMCustomUCFFile = NULL;
                      FSMCustomUCFFileLength = 0;
                    }
                    FSMCustomUCFFile = (ucf_line_t *)calloc(Length >> 2, sizeof(ucf_line_t));
                    if (FSMCustomUCFFile == NULL)
                    {
                      STBOX1_PRINTF("Error in memory allocation FSMCustomUCFFile\r\n");
                    }
                    else
                    {
                      FSMCustomUCFFileLength = Length >> 2;
                      FromHexToUCF(regConfig, Length, FSMCustomUCFFile);
                    }
                  }
                }
                fsmLabels = json_object_dotget_string(Sensor, "configuration.fsmLabels");
                if (fsmLabels != NULL)
                {
                  uint32_t Length = strlen(fsmLabels);
                  STBOX1_PRINTF("\t\t\tFSM Labels [%ld] found\r\n", Length);
                  /* Allocate the Memory for the Custom Labels for FSM */
                  extern char *FSMCustomLabels;
                  if (FSMCustomLabels != NULL)
                  {
                    /* if there is already one FSMCustomLabels...Release the Memory before */
                    free(FSMCustomLabels);
                    FSMCustomLabels = NULL;
                  }
                  FSMCustomLabels = (char *)calloc(Length + 1, sizeof(char));
                  if (FSMCustomLabels == NULL)
                  {
                    STBOX1_PRINTF("Error in memory allocation FSMCustomLabels\r\n");
                  }
                  else
                  {
                    FSMCustomLabelsLength = Length + 1;
                    memcpy(FSMCustomLabels, fsmLabels, Length);
                    /* Put Termination */
                    FSMCustomLabels[Length] = '\n';
                  }
                }
              }
            }
          }
        }
      }
      json_value_free(root_value);
      STBOX1_PRINTF("%ld Alloc Not Released\r\nTotal Mem Used=%ld\r\n", malloc_count, malloc_size);
      STBOX1_PRINTF("--- Json End Parsing ---\r\n");
      /* Return Message */
      bytes_to_write = sprintf((char *)buffer_to_write, "Flow_parse_ok");
      term_update(buffer_to_write, bytes_to_write);
    }
  }

  return SendBackData;
}

/**
  * @brief  This function makes the parsing of the Debug Console Commands
  * @param  uint8_t *att_data attribute data
  * @param  uint8_t data_length length of the data
  * @param uint32_t *DecodingOneStream Flag for understanding when we are decoding one Stream
  * @param int32_t *StreamLength Length of the application stream
  * @param uint8_t **CompressedData pointer to buffer for storing the compressed json
  * @retval uint32_t SendBackData true/false
  */
uint32_t DebugConsoleCommandParsing(uint8_t *att_data, uint8_t data_length, uint32_t *DecodingOneStream,
                                    int32_t *StreamLength, uint8_t **CompressedData)
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
                             "uid\n"
                             "getFSMLabels\n"
                             "getMLCLabels\n"
                             "delMLCCustom\n"
                             "delFSMCustom\n");
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
                             "\tSTEVAL-MKBOXPRO Rev %c board"
                             "\n",
                             STBOX1_PACKAGENAME,
                             STBOX1_VERSION_MAJOR, STBOX1_VERSION_MINOR, STBOX1_VERSION_PATCH,
                             'C');
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
  else if (!strncmp("getFSMLabels", (char *)(att_data), 12))
  {
    if (FSMCustomLabels == NULL)
    {
      bytes_to_write =
        sprintf((char *)buffer_to_write,
                "<FSM_OUTS1>4D PosRec,0='N/A',16='Portrait Down',32='Portrait Up'"
                "64='Landscape Right',128='Landscape Left';\n");
      term_update(buffer_to_write, bytes_to_write);
    }
    else
    {
      uint32_t Counter;
      for (Counter = 0; Counter < FSMCustomLabelsLength; Counter += max_ble_char_std_out_len)
      {
        uint32_t MinSize;
        MinSize = FSMCustomLabelsLength - Counter;
        MinSize = (MinSize > max_ble_char_std_out_len) ?  max_ble_char_std_out_len : MinSize;
        term_update((uint8_t *)(FSMCustomLabels + Counter), MinSize);
      }
    }
    SendBackData = 0;
  }
  else if (!strncmp("getMLCLabels", (char *)(att_data), 12))
  {
    if (MLCCustomLabels == NULL)
    {
      bytes_to_write = sprintf((char *)buffer_to_write,
                               "<MLC0>Activity Rec,1='Other',4='Walking',8='Running',12='Driving';\n");
      term_update(buffer_to_write, bytes_to_write);
    }
    else
    {
      uint32_t Counter;
      for (Counter = 0; Counter < MLCCustomLabelsLength; Counter += max_ble_char_std_out_len)
      {
        uint32_t MinSize;
        MinSize = MLCCustomLabelsLength - Counter;
        MinSize = (MinSize > max_ble_char_std_out_len) ?  max_ble_char_std_out_len : MinSize;
        term_update((uint8_t *)(MLCCustomLabels + Counter), MinSize);
      }
    }
    SendBackData = 0;
  }
  else if (!strncmp("delMLCCustom", (char *)(att_data), 12))
  {
    if (MLCCustomLabels != NULL)
    {
      free(MLCCustomLabels);
      MLCCustomLabels = NULL;
      MLCCustomLabelsLength = 0;
      bytes_to_write = sprintf((char *)buffer_to_write, "MLCCustomLabels Deleted\n");
      term_update(buffer_to_write, bytes_to_write);
    }
    if (MLCCustomUCFFile != NULL)
    {
      free(MLCCustomUCFFile);
      MLCCustomUCFFile = NULL;
      MLCCustomUCFFileLength = 0;
      bytes_to_write = sprintf((char *)buffer_to_write, "MLCCustomUCFFile Deleted\n");
      term_update(buffer_to_write, bytes_to_write);
    }
    SendBackData = 0;
  }
  else if (!strncmp("delFSMCustom", (char *)(att_data), 12))
  {
    if (FSMCustomLabels != NULL)
    {
      free(FSMCustomLabels);
      FSMCustomLabels = NULL;
      FSMCustomLabelsLength = 0;
      bytes_to_write = sprintf((char *)buffer_to_write, "FSMCustomLabels Deleted\n");
      term_update(buffer_to_write, bytes_to_write);
    }
    if (FSMCustomUCFFile != NULL)
    {
      free(FSMCustomUCFFile);
      FSMCustomUCFFile = NULL;
      FSMCustomUCFFileLength = 0;
      bytes_to_write = sprintf((char *)buffer_to_write, "FSMCustomUCFFile Deleted\n");
      term_update(buffer_to_write, bytes_to_write);
    }
    SendBackData = 0;
  }
  else if (!strncmp("SF", (char *)(att_data), 2))
  {
    uint32_t TimeStamp;
    uint8_t *PointerByte = (uint8_t *) StreamLength;
    PointerByte[0] = att_data[5];
    PointerByte[1] = att_data[4];
    PointerByte[2] = att_data[3];
    PointerByte[3] = att_data[2];

    PointerByte = (uint8_t *) &TimeStamp;
    PointerByte[0] = att_data[9];
    PointerByte[1] = att_data[8];
    PointerByte[2] = att_data[7];
    PointerByte[3] = att_data[6];

    /* Debug Message */
    STBOX1_PRINTF("SF command Length=%ld, TS=%lu\r\n", *StreamLength, TimeStamp);

    /* Alloc buffer for storing compressed json */
    *CompressedData = (uint8_t *) malloc((*StreamLength) * sizeof(uint8_t));
    if ((*CompressedData) == NULL)
    {
      STBOX1_PRINTF("Memory Allocation error for CompressedData\r\n");
      /* Return Message */
      bytes_to_write = sprintf((char *)buffer_to_write, "Allocation Error");
      term_update(buffer_to_write, bytes_to_write);
    }
    else
    {
      /* Return Message */
      bytes_to_write = sprintf((char *)buffer_to_write, "Flow_Req_Received");
      term_update(buffer_to_write, bytes_to_write);
    }

    SendBackData = 0;
    *DecodingOneStream = 1;
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

  /* Make the device connectable again */

  /* Reset for any problem during FOTA update */
  SizeOfUpdateBlueFW = 0;

  /*Stop all the timers */

  if (W2ST_CHECK_CONNECTION(W2ST_CONNECT_ACC_GYRO_MAG))
  {
    if (HAL_TIM_OC_Stop_IT(&TIM_CC_HANDLE, TIM_CHANNEL_3) != HAL_OK)
    {
      /* Stopping Error */
      STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
    }
    STBOX1_PRINTF("Stop Iner\r\n");
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
  * @brief This function Decompresses one buffer
  * @param uint8_t *compressed buffer
  * @param uint32_t size dimension of the compressed buffer
  * @param uint32_t *UnComSize Size of uncompressed buffer
  * @retval uint8_t *Pointer to uncompressed buffer
  */
static uint8_t *Decompress(uint8_t *compressed, uint32_t size, uint32_t *UnComSize)
{
  struct uzlib_uncomp dest_struct_t;
  uint8_t *uncompressed;
  int32_t res;
  uint32_t un_size;
  uint32_t chunk_len;

#define OUT_CHUNK_SIZE 1

  un_size = GetUncompressedSize(compressed, size);
  *UnComSize = un_size;
  STBOX1_PRINTF("Uncompressed Size =%ld\r\n", un_size);

  uncompressed = (uint8_t *)calloc(un_size, sizeof(uint8_t));
  if (uncompressed == NULL)
  {
    STBOX1_PRINTF("Error in memory allocation for decompression\r\n");
    return NULL;
  }

  uzlib_uncompress_init(&dest_struct_t, NULL, 0);

  dest_struct_t.source = compressed;
  dest_struct_t.source_limit = compressed + size - 4;
  dest_struct_t.source_read_cb = NULL;

  res = uzlib_gzip_parse_header(&dest_struct_t);

  if (res != TINF_OK)
  {
    STBOX1_PRINTF("Error in decompressing regConfig\r\n");
    return NULL;
  }

  dest_struct_t.dest_start = dest_struct_t.dest = uncompressed;

  chunk_len = 0;

  while (un_size)
  {
    chunk_len = un_size < OUT_CHUNK_SIZE ? un_size : OUT_CHUNK_SIZE;
    dest_struct_t.dest_limit = dest_struct_t.dest + chunk_len;
    res = uzlib_uncompress_chksum(&dest_struct_t);
    un_size -= chunk_len;
    if (res != TINF_OK)
    {
      break;
    }
  }

  if (res != TINF_DONE)
  {
    STBOX1_PRINTF("Error in decompressing regConfig\r\n");
    return NULL;
  }
  return uncompressed;
}

/**
  * @brief This function return the size of uncompressed buffer
  * @param uint8_t *compressed buffer
  * @param uint32_t size dimension of the compressed buffer
  * @retval uint32_t size of uncompressed buffer
  */
static uint32_t GetUncompressedSize(uint8_t *compressed, uint32_t size)
{
  uint32_t dlen =     compressed[size - 1];
  dlen = (256 * dlen) + compressed[size - 2];
  dlen = (256 * dlen) + compressed[size - 3];
  dlen = (256 * dlen) + compressed[size - 4];
  return dlen + 1;
}

#ifdef STBOX1_ENABLE_PRINTF
/* For searching how many bytes are allocated and if the code release all the memory */
static void *counted_malloc(size_t size)
{
  void *res = malloc(size);
  if (res != NULL)
  {
    malloc_count++;
    malloc_size += size;
  }
  return res;
}

static void counted_free(void *ptr)
{
  if (ptr != NULL)
  {
    malloc_count--;
  }
  free(ptr);
}
#endif /* STBOX1_ENABLE_PRINTF */

/**
  * @brief  This function Converts one string to UCF program
  * @param const char *In Input char string
  * @param uint32_t len length of the input char string
  * @param ucf_line_t *UCFProgram pointer to output UCF program
  * @retval None
  */
static void FromHexToUCF(const char *In, uint32_t len, ucf_line_t *UCFProgram)
{
  uint32_t i;
  uint32_t AH;
  uint32_t AL;
  uint32_t DH;
  uint32_t DL;

  for (i = 0; i < len; i += 4)
  {
    char In1 = *In++;
    char In2 = *In++;
    char In3 = *In++;
    char In4 = *In++;
    AH = (In1 > '9') ? (In1 - 'A' + 10) : (In1 - '0');
    AL = (In2 > '9') ? (In2 - 'A' + 10) : (In2 - '0');
    DH = (In3 > '9') ? (In3 - 'A' + 10) : (In3 - '0');
    DL = (In4 > '9') ? (In4 - 'A' + 10) : (In4 - '0');
    UCFProgram->address = (AH << 4) | AL;
    UCFProgram->data    = (DH << 4) | DL;
    UCFProgram++;
  }
}

/**************************************************************
  * Callback functions to manage the notify/read/write events *
  *************************************************************/

/**
  * @brief  Callback Function for Un/Subscription Inertial Feature
  * @param  ble_notify_event_t Event Sub/Unsub
  * @retval None
  */
void notify_event_inertial(ble_notify_event_t Event)
{
  if (Event == BLE_NOTIFY_SUB)
  {
    uint32_t uhCapture = __HAL_TIM_GET_COUNTER(&TIM_CC_HANDLE);
    W2ST_ON_CONNECTION(W2ST_CONNECT_ACC_GYRO_MAG);

    /* Initialise the Acc/Gyro no MLC o FSM */
    InitAcc();

    /* Start the TIM Base generation in interrupt mode */
    if (HAL_TIM_OC_Start_IT(&TIM_CC_HANDLE, TIM_CHANNEL_3) != HAL_OK)
    {
      /* Starting Error */
      STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
    }

    /* Set the Capture Compare Register value */
    __HAL_TIM_SET_COMPARE(&TIM_CC_HANDLE, TIM_CHANNEL_3, (uhCapture + STBOX1_UPDATE_INV));
    STBOX1_PRINTF("Start Iner\r\n");
  }
  else if (Event == BLE_NOTIFY_UNSUB)
  {
    W2ST_OFF_CONNECTION(W2ST_CONNECT_ACC_GYRO_MAG);

    DeInit_Acc();

    /* Stop the TIM Base generation in interrupt mode */
    if (HAL_TIM_OC_Stop_IT(&TIM_CC_HANDLE, TIM_CHANNEL_3) != HAL_OK)
    {
      /* Stopping Error */
      STBOX1_Error_Handler(STBOX1_ERROR_TIMER, __FILE__, __LINE__);
    }
    STBOX1_PRINTF("Stop Iner\r\n");
  }
}

/**
  * @brief  Callback Function for Un/Subscription MLC Feature
  * @param  ble_notify_event_t Event Sub/Unsub
  * @retval None
  */
void notify_event_machine_learning_core(ble_notify_event_t Event)
{
  if (Event == BLE_NOTIFY_SUB)
  {
    W2ST_ON_CONNECTION(W2ST_CONNECT_MLC);

    InitAcc_MLC(1);

    STBOX1_PRINTF("Start MLC\r\n");
  }
  else if (Event == BLE_NOTIFY_UNSUB)
  {
    W2ST_OFF_CONNECTION(W2ST_CONNECT_MLC);

    DeInit_Acc();

    STBOX1_PRINTF("Stop MLC\r\n");
  }
}

/**
  * @brief  Callback Function for Un/Subscription FSM Feature
  * @param  ble_notify_event_t Event Sub/Unsub
  * @retval None
  */
void notify_event_finite_state_machine(ble_notify_event_t Event)
{
  if (Event == BLE_NOTIFY_SUB)
  {
    W2ST_ON_CONNECTION(W2ST_CONNECT_FSM);
    InitAcc_FSM(0);

    STBOX1_PRINTF("Start FSM\r\n");
  }
  else if (Event == BLE_NOTIFY_UNSUB)
  {
    W2ST_OFF_CONNECTION(W2ST_CONNECT_FSM);
    STBOX1_PRINTF("Stop FSM\r\n");
  }
}

/**
  * @brief  Read request from Activity Recognition characteristic
  * @param  ble_ar_output_t activity_code Activity Recognized
  * @param  ble_ar_algo_idx_t algorithm Code
  * @retval None
  */
void read_request_activity_recognition_function(ble_ar_output_t *activity_code, ble_ar_algo_idx_t *algorithm)
{
  *activity_code = ActivityCode;
  *algorithm = HAR_ALGO_IDX_NONE;
}

/**
  * @brief  Read request from Machine Learning Core characteristic
  * @param  uint8_t *mlc_out output of the MLC
  * @param  uint8_t *mlc_status_mainpage pointer to the MLC status mainpage
  * @retval None
  */
void read_request_machine_learning_core_function(uint8_t *mlc_out, uint8_t *mlc_status_mainpage)
{
  lsm6dsv16x_all_sources_t status;
  lsm6dsv16x_all_sources_get(LSM6DSV16X_CONTEX, &status);

  *mlc_status_mainpage = (status.mlc1) | (status.mlc2 << 1) | (status.mlc3 << 2) | (status.mlc4 << 3);
  lsm6dsv16x_mlc_out_get(LSM6DSV16X_CONTEX, (lsm6dsv16x_mlc_out_t *) mlc_out);
}

/**
  * @brief  Read request from Finite State Machine characteristic
  * @param  uint8_t *fsm_out output of the FSM
  * @param  uint8_t *fsm_status_a_mainpage pointer to the FSM status mainpage A
  * @param  uint8_t *fsm_status_b_mainpage pointer to the FSM status mainpage B
  * @retval None
  */
void read_request_finite_state_machine_function(uint8_t *fsm_out,
                                                uint8_t *fsm_status_a_mainpage,
                                                uint8_t *fsm_status_b_mainpage)
{
  lsm6dsv16x_all_sources_t      status;
  lsm6dsv16x_all_sources_get(LSM6DSV16X_CONTEX, &status);

  *fsm_status_a_mainpage = ((status.fsm1) | (status.fsm2 << 1) | (status.fsm3 << 2) | (status.fsm4 << 3) |
                            (status.fsm5 << 4) | (status.fsm6 << 5) | (status.fsm7 << 6) | (status.fsm8 << 7));

  *fsm_status_b_mainpage = 0; /* Dummy */

  lsm6dsv16x_fsm_out_get(LSM6DSV16X_CONTEX, (lsm6dsv16x_fsm_out_t *)fsm_out);
}

/**
  * @brief  Callback Function for Un/Subscription Activity Rec Feature
  * @param  ble_notify_event_t Event Sub/Unsub
  * @retval None
  */
void notify_event_activity_recognition(ble_notify_event_t Event)
{
  if (Event == BLE_NOTIFY_SUB)
  {
    W2ST_ON_CONNECTION(W2ST_CONNECT_AR_EVENT);

    InitAcc_MLC(1);

    STBOX1_PRINTF("Start Activity Rec\r\n");
  }
  else if (Event == BLE_NOTIFY_UNSUB)
  {
    W2ST_OFF_CONNECTION(W2ST_CONNECT_AR_EVENT);

    DeInit_Acc();

    STBOX1_PRINTF("Stop Activity Rec\r\n");
  }
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
          "\tSTEVAL-MKBOXPRO Rev %c board"
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
          'C',
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
  UpdateCurrFlashBankFwIdBoardName(STBOX1C_BLUEST_SDK_FW_ID, NewName);

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

